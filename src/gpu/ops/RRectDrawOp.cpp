/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "RRectDrawOp.h"
#include "core/DataSource.h"
#include "core/utils/MathExtra.h"
#include "gpu/GpuBuffer.h"
#include "gpu/ProxyProvider.h"
#include "gpu/ResourceProvider.h"
#include "gpu/VertexProvider.h"
#include "gpu/processors/EllipseGeometryProcessor.h"
#include "tgfx/core/RenderFlags.h"

namespace tgfx {
// We have three possible cases for geometry for a round rect.
//
// In the case of a normal fill or a stroke, we draw the round rect as a 9-patch:
//    ____________
//   |_|________|_|
//   | |        | |
//   | |        | |
//   | |        | |
//   |_|________|_|
//   |_|________|_|
//
// For strokes, we don't draw the center quad.
//
// For circular round rects, in the case where the stroke width is greater than twice
// the corner radius (over stroke), we add additional geometry to mark out the rectangle
// in the center. The shared vertices are duplicated, so we can set a different outer radius
// for the fill calculation.
//    ____________
//   |_|________|_|
//   | |\ ____ /| |
//   | | |    | | |
//   | | |____| | |
//   |_|/______\|_|
//   |_|________|_|
//
// We don't draw the center quad from the fill rect in this case.
//
// For filled rrects that need to provide a distance vector we reuse the overstroke
// geometry but make the inner rect degenerate (either a point or a horizontal or
// vertical line).

class RRectVertexProvider : public VertexProvider {
 public:
  RRectVertexProvider(std::vector<PlacementPtr<RRectPaint>> rRectPaints, AAType aaType,
                      bool useScale)
      : rRectPaints(std::move(rRectPaints)), aaType(aaType), useScale(useScale) {
  }

  size_t vertexCount() const override {
    auto floatCount = rRectPaints.size() * 4 * 36;
    if (useScale) {
      floatCount += rRectPaints.size() * 4 * 4;
    }
    return floatCount;
  }

  void getVertices(float* vertices) const override {
    auto index = 0;
    // Pre-calculate the aaBloat value once
    const float aaBloat = aaType == AAType::MSAA ? FLOAT_SQRT2 : .5f;
    
    for (auto& rRectPaint : rRectPaints) {
      auto viewMatrix = rRectPaint->viewMatrix;
      auto rRect = rRectPaint->rRect;
      const Color& color = rRectPaint->color;
      
      // Pre-pack the color once per rRect to avoid multiple conversions
      uint32_t packedColor = 
          (static_cast<uint32_t>(color.red * 255.0f + 0.5f) << 0) |
          (static_cast<uint32_t>(color.green * 255.0f + 0.5f) << 8) |
          (static_cast<uint32_t>(color.blue * 255.0f + 0.5f) << 16) |
          (static_cast<uint32_t>(color.alpha * 255.0f + 0.5f) << 24);
      
      // Perform scaling once per rRect
      auto scales = viewMatrix.getAxisScales();
      
      // Inline scaling operations
      rRect.rect.left *= scales.x;
      rRect.rect.right *= scales.x;
      rRect.rect.top *= scales.y;
      rRect.rect.bottom *= scales.y;
      rRect.radii.x *= scales.x;
      rRect.radii.y *= scales.y;
      
      const float invScaleX = 1.0f / scales.x;
      const float invScaleY = 1.0f / scales.y;
      viewMatrix.preScale(invScaleX, invScaleY);
      
      // Pre-calculate reciprocal radii
      float reciprocalRadii[4] = {1e6f, 1e6f, 1e6f, 1e6f};
      float rxRecip = 1e6f, ryRecip = 1e6f;
      if (rRect.radii.x > 0) {
        rxRecip = 1.f / rRect.radii.x;
        reciprocalRadii[0] = rxRecip;
      }
      if (rRect.radii.y > 0) {
        ryRecip = 1.f / rRect.radii.y;
        reciprocalRadii[1] = ryRecip;
      }
      
      // Calculate outer radii and bounds once
      const float rx = rRect.radii.x;
      const float ry = rRect.radii.y;
      float xOuterRadius = rx + aaBloat;
      float yOuterRadius = ry + aaBloat;

      float xMaxOffset = rx > 0.0f ? xOuterRadius * rxRecip : 1e6f;
      float yMaxOffset = ry > 0.0f ? yOuterRadius * ryRecip : 1e6f;
      
      // Inline the bounds makeOutset operation
      Rect bounds = rRect.rect;
      bounds.left -= aaBloat;
      bounds.top -= aaBloat;
      bounds.right += aaBloat;
      bounds.bottom += aaBloat;
      
      float yCoords[4] = {bounds.top, bounds.top + yOuterRadius, bounds.bottom - yOuterRadius,
                          bounds.bottom};
      float yOuterOffsets[4] = {
          yMaxOffset,
          FLOAT_NEARLY_ZERO,  // we're using inversesqrt() in shader, so can't be exactly 0
          FLOAT_NEARLY_ZERO, yMaxOffset};
      
      const float maxRadius = rx > ry ? rx : ry;
      
      // Cache x-coordinate values for reuse
      float xCoords[4] = {bounds.left, bounds.left + xOuterRadius, 
                          bounds.right - xOuterRadius, bounds.right};
      
      // Process each y-coordinate
      for (int i = 0; i < 4; ++i) {
        const float y = yCoords[i];
        const float yOffset = yOuterOffsets[i];
        
        // Create points for this row to transform
        Point points[4];
        for (int j = 0; j < 4; ++j) {
          points[j].x = xCoords[j];
          points[j].y = y;
        }
        
        // Transform all points at once
        viewMatrix.mapPoints(points, points, 4);
        
        // First point (left edge)
        vertices[index++] = points[0].x; 
        vertices[index++] = points[0].y;
        *reinterpret_cast<uint32_t*>(&vertices[index++]) = packedColor;
        vertices[index++] = xMaxOffset;
        vertices[index++] = yOffset;
        if (useScale) {
          vertices[index++] = maxRadius;
        }
        vertices[index++] = reciprocalRadii[0];
        vertices[index++] = reciprocalRadii[1];
        vertices[index++] = reciprocalRadii[2];
        vertices[index++] = reciprocalRadii[3];

        // Second point (left-inner edge)
        vertices[index++] = points[1].x;
        vertices[index++] = points[1].y;
        *reinterpret_cast<uint32_t*>(&vertices[index++]) = packedColor;
        vertices[index++] = FLOAT_NEARLY_ZERO;
        vertices[index++] = yOffset;
        if (useScale) {
          vertices[index++] = maxRadius;
        }
        vertices[index++] = reciprocalRadii[0];
        vertices[index++] = reciprocalRadii[1];
        vertices[index++] = reciprocalRadii[2];
        vertices[index++] = reciprocalRadii[3];

        // Third point (right-inner edge)
        vertices[index++] = points[2].x;
        vertices[index++] = points[2].y;
        *reinterpret_cast<uint32_t*>(&vertices[index++]) = packedColor;
        vertices[index++] = FLOAT_NEARLY_ZERO;
        vertices[index++] = yOffset;
        if (useScale) {
          vertices[index++] = maxRadius;
        }
        vertices[index++] = reciprocalRadii[0];
        vertices[index++] = reciprocalRadii[1];
        vertices[index++] = reciprocalRadii[2];
        vertices[index++] = reciprocalRadii[3];

        // Fourth point (right edge)
        vertices[index++] = points[3].x;
        vertices[index++] = points[3].y;
        *reinterpret_cast<uint32_t*>(&vertices[index++]) = packedColor;
        vertices[index++] = xMaxOffset;
        vertices[index++] = yOffset;
        if (useScale) {
          vertices[index++] = maxRadius;
        }
        vertices[index++] = reciprocalRadii[0];
        vertices[index++] = reciprocalRadii[1];
        vertices[index++] = reciprocalRadii[2];
        vertices[index++] = reciprocalRadii[3];
      }
    }
  }

 private:
  std::vector<PlacementPtr<RRectPaint>> rRectPaints = {};
  AAType aaType = AAType::None;
  bool useScale = false;
};

static bool UseScale(Context* context) {
  return !context->caps()->floatIs32Bits;
}

PlacementPtr<RRectDrawOp> RRectDrawOp::Make(Context* context,
                                            std::vector<PlacementPtr<RRectPaint>> rects,
                                            AAType aaType, uint32_t renderFlags) {
  if (rects.empty()) {
    return nullptr;
  }
  auto rectSize = rects.size();
  auto drawOp = context->drawingBuffer()->make<RRectDrawOp>(aaType, rectSize);
  drawOp->indexBufferProxy = context->resourceProvider()->rRectIndexBuffer();
  auto useScale = UseScale(context);
  auto vertexProvider = std::make_unique<RRectVertexProvider>(std::move(rects), aaType, useScale);
  if (rectSize <= 1) {
    // If we only have one rect, it is not worth the async task overhead.
    renderFlags |= RenderFlags::DisableAsyncTask;
  }
  auto sharedVertexBuffer =
      context->proxyProvider()->createSharedVertexBuffer(std::move(vertexProvider), renderFlags);
  drawOp->vertexBufferProxy = sharedVertexBuffer.first;
  drawOp->vertexBufferOffset = sharedVertexBuffer.second;
  return drawOp;
}

RRectDrawOp::RRectDrawOp(AAType aaType, size_t rectCount) : DrawOp(aaType), rectCount(rectCount) {
}

void RRectDrawOp::execute(RenderPass* renderPass) {
  if (indexBufferProxy == nullptr) {
    return;
  }
  auto indexBuffer = indexBufferProxy->getBuffer();
  if (indexBuffer == nullptr) {
    return;
  }
  std::shared_ptr<GpuBuffer> vertexBuffer =
      vertexBufferProxy ? vertexBufferProxy->getBuffer() : nullptr;
  if (vertexBuffer == nullptr) {
    return;
  }
  auto renderTarget = renderPass->renderTarget();
  auto drawingBuffer = renderPass->getContext()->drawingBuffer();
  auto gp =
      EllipseGeometryProcessor::Make(drawingBuffer, renderTarget->width(), renderTarget->height(),
                                     false, UseScale(renderPass->getContext()));
  auto pipeline = createPipeline(renderPass, std::move(gp));
  renderPass->bindProgramAndScissorClip(pipeline.get(), scissorRect());
  renderPass->bindBuffers(indexBuffer, vertexBuffer, vertexBufferOffset);
  auto numIndicesPerRRect = ResourceProvider::NumIndicesPerRRect();
  renderPass->drawIndexed(PrimitiveType::Triangles, 0, rectCount * numIndicesPerRRect);
}
}  // namespace tgfx
