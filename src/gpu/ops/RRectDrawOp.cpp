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

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

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

#ifdef __ARM_NEON
  // Transform using NEON SIMD instructions for ARM
  static inline void transformPoints_NEON(const Matrix& matrix, Point* dst, const Point* src, int count) {
    // Use public methods of Matrix instead of accessing private members directly
    for (int i = 0; i < count; i += 4) {
      // Process points in batches of 4 for SIMD efficiency
      int remaining = std::min(4, count - i);
      matrix.mapPoints(dst + i, src + i, remaining);
    }
  }
#endif

  void getVertices(float* vertices) const override {
    auto index = 0;
    // Pre-calculate the aaBloat value once
    const float aaBloat = aaType == AAType::MSAA ? FLOAT_SQRT2 : .5f;
    
    // Fast path for single rectangle (most common case)
    if (rRectPaints.size() == 1) {
      auto& rRectPaint = rRectPaints[0];
      auto rRect = rRectPaint->rRect;
      const Color& color = rRectPaint->color;
      
      // Pre-pack color once (using bit shifts for better performance)
      uint32_t packedColor = 
          (static_cast<uint32_t>(color.red * 255.0f) & 0xFF) |
          ((static_cast<uint32_t>(color.green * 255.0f) & 0xFF) << 8) |
          ((static_cast<uint32_t>(color.blue * 255.0f) & 0xFF) << 16) |
          ((static_cast<uint32_t>(color.alpha * 255.0f) & 0xFF) << 24);
      
      // Inline matrix computation for better performance
      auto& viewMatrix = rRectPaint->viewMatrix;
      
      // Pre-compute all coordinates for the rectangle corners
      const float rx = rRect.radii.x;
      const float ry = rRect.radii.y;
      
      // Pre-compute reciprocal values once
      float rxRecip = rx > 0.0f ? 1.0f / rx : 1e6f;
      float ryRecip = ry > 0.0f ? 1.0f / ry : 1e6f;
      
      // Pre-compute outer radius values
      const float xOuterRadius = rx + aaBloat;
      const float yOuterRadius = ry + aaBloat;
      
      // Pre-compute max offset values
      const float xMaxOffset = rx > 0.0f ? xOuterRadius * rxRecip : 1.0f;
      const float yMaxOffset = ry > 0.0f ? yOuterRadius * ryRecip : 1.0f;
      
      // Calculate corner positions with bloat
      const float bloatedLeft = rRect.rect.left - aaBloat;
      const float bloatedTop = rRect.rect.top - aaBloat;
      const float bloatedRight = rRect.rect.right + aaBloat;
      const float bloatedBottom = rRect.rect.bottom + aaBloat;
      
      // Calculate the offsets from outer corners to rounded corners
      const float leftOffset = rRect.rect.left + xOuterRadius - bloatedLeft;
      const float topOffset = rRect.rect.top + yOuterRadius - bloatedTop;
      const float rightOffset = bloatedRight - (rRect.rect.right - xOuterRadius);
      const float bottomOffset = bloatedBottom - (rRect.rect.bottom - yOuterRadius);
      
      // Cache coordinates for corners
      Point cornerPoints[16];
      
      // Top-left block (4 points)
      cornerPoints[0].set(bloatedLeft, bloatedTop);
      cornerPoints[1].set(bloatedLeft + leftOffset, bloatedTop);
      cornerPoints[2].set(bloatedRight - rightOffset, bloatedTop);
      cornerPoints[3].set(bloatedRight, bloatedTop);
      
      // Top-right block (4 points)
      cornerPoints[4].set(bloatedLeft, bloatedTop + topOffset);
      cornerPoints[5].set(bloatedLeft + leftOffset, bloatedTop + topOffset);
      cornerPoints[6].set(bloatedRight - rightOffset, bloatedTop + topOffset);
      cornerPoints[7].set(bloatedRight, bloatedTop + topOffset);
      
      // Bottom-left block (4 points)
      cornerPoints[8].set(bloatedLeft, bloatedBottom - bottomOffset);
      cornerPoints[9].set(bloatedLeft + leftOffset, bloatedBottom - bottomOffset);
      cornerPoints[10].set(bloatedRight - rightOffset, bloatedBottom - bottomOffset);
      cornerPoints[11].set(bloatedRight, bloatedBottom - bottomOffset);
      
      // Bottom-right block (4 points)
      cornerPoints[12].set(bloatedLeft, bloatedBottom);
      cornerPoints[13].set(bloatedLeft + leftOffset, bloatedBottom);
      cornerPoints[14].set(bloatedRight - rightOffset, bloatedBottom);
      cornerPoints[15].set(bloatedRight, bloatedBottom);
      
      // Transform all points at once using optimized method
      viewMatrix.mapPoints(cornerPoints, cornerPoints, 16);
      
      // Cache these values for better performance in the inner loop
      const float maxRadius = std::max(rx, ry);
      
      // Repack the offset array for better memory locality
      const float cornerOffsets[4] = {
          xMaxOffset, 0.0f, 0.0f, xMaxOffset
      };
      
      const float edgeOffsets[4] = {
          yMaxOffset, 0.0f, 0.0f, yMaxOffset
      };
      
      // Pack radii for faster access
      const float reciprocalRadii[4] = {rxRecip, ryRecip, rxRecip, ryRecip};
      
      // SIMD-friendly buffer pointers for better memory access pattern
      float* vPtr = vertices;
      
#ifdef __ARM_NEON
      // Use NEON SIMD when available for faster vertex generation
      float32x4_t vPackedColor = vdupq_n_f32(*reinterpret_cast<const float*>(&packedColor));
      float32x4_t vRecipRadii = vld1q_f32(reciprocalRadii);
      
      for (int i = 0; i < 16; i++) {
        const Point& point = cornerPoints[i];
        
        // Position
        vPtr[0] = point.x;
        vPtr[1] = point.y;
        
        // Color
        vst1q_lane_f32(vPtr + 2, vPackedColor, 0);
        
        // Offsets
        int xIndex = i & 3; // i % 4
        int yIndex = i >> 2; // i / 4
        vPtr[3] = cornerOffsets[xIndex];
        vPtr[4] = edgeOffsets[yIndex];
        
        // Optional scale
        if (useScale) {
          vPtr[5] = maxRadius;
          // Radii (using SIMD)
          vst1q_f32(vPtr + 6, vRecipRadii);
          vPtr += 10; // Position(2) + Color(1) + Offsets(2) + Scale(1) + Radii(4)
        } else {
          // Radii (using SIMD)
          vst1q_f32(vPtr + 5, vRecipRadii);
          vPtr += 9; // Position(2) + Color(1) + Offsets(2) + Radii(4)
        }
      }
#else
      // Non-SIMD fallback with optimized memory layout
      for (int i = 0; i < 16; i++) {
        const Point& point = cornerPoints[i];
        
        // Position
        *vPtr++ = point.x;
        *vPtr++ = point.y;
        
        // Color (store directly as uint32)
        *reinterpret_cast<uint32_t*>(vPtr) = packedColor;
        vPtr++;
        
        // Offsets
        int xIndex = i & 3; // i % 4
        int yIndex = i >> 2; // i / 4
        *vPtr++ = cornerOffsets[xIndex];
        *vPtr++ = edgeOffsets[yIndex];
        
        // Optional scale
        if (useScale) {
          *vPtr++ = maxRadius;
        }
        
        // Radii
        *vPtr++ = reciprocalRadii[0];
        *vPtr++ = reciprocalRadii[1];
        *vPtr++ = reciprocalRadii[2];
        *vPtr++ = reciprocalRadii[3];
      }
#endif
      
      return;
    }
    
    // Multi-rectangle path
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
      
      // Perform scaling once per rRect and cache results
      // auto scales = viewMatrix.getAxisScales();
      
      // Scale rectangle bounds
      const float rectLeft = rRect.rect.left;
      const float rectRight = rRect.rect.right;
      const float rectTop = rRect.rect.top;
      const float rectBottom = rRect.rect.bottom;
      
      // Scale radii
      const float rx = rRect.radii.x;
      const float ry = rRect.radii.y;
      
      // Calculate reciprocal radii once
      const float rxRecip = rx > 0.0f ? 1.0f / rx : 1e6f;
      const float ryRecip = ry > 0.0f ? 1.0f / ry : 1e6f;
      
      // Cache reciprocal radii as array
      const float reciprocalRadii[4] = {rxRecip, ryRecip, rxRecip, ryRecip};
      
      // Pre-compute outer radii
      const float xOuterRadius = rx + aaBloat;
      const float yOuterRadius = ry + aaBloat;
      
      // Pre-compute offset values
      const float xMaxOffset = rx > 0.0f ? xOuterRadius * rxRecip : 1e6f;
      const float yMaxOffset = ry > 0.0f ? yOuterRadius * ryRecip : 1e6f;
      
      // Calculate and cache final bounds
      const float boundsLeft = rectLeft - aaBloat;
      const float boundsTop = rectTop - aaBloat;
      const float boundsRight = rectRight + aaBloat;
      const float boundsBottom = rectBottom + aaBloat;
      
      // Pre-compute all coordinate values
      const float xCoords[4] = {
          boundsLeft, 
          boundsLeft + xOuterRadius, 
          boundsRight - xOuterRadius, 
          boundsRight
      };
      
      const float yCoords[4] = {
          boundsTop, 
          boundsTop + yOuterRadius, 
          boundsBottom - yOuterRadius,
          boundsBottom
      };
      
      // Fixed offsets for better SIMD utilization
      const float xOffsets[4] = {
          xMaxOffset,
          FLOAT_NEARLY_ZERO,
          FLOAT_NEARLY_ZERO,
          xMaxOffset
      };
      
      const float yOffsets[4] = {
          yMaxOffset,
          FLOAT_NEARLY_ZERO,
          FLOAT_NEARLY_ZERO, 
          yMaxOffset
      };
      
      // Calculate max radius once
      const float maxRadius = rx > ry ? rx : ry;
      
      // Adjust the view matrix once per rRect
      const float invScaleX = 1.0f;
      const float invScaleY = 1.0f;
      viewMatrix.preScale(invScaleX, invScaleY);
      
      // Process all 16 points at once for better vectorization
      Point points[16];
      int pointIdx = 0;
      
      // Set coordinates for all points
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
          points[pointIdx].x = xCoords[j];
          points[pointIdx].y = yCoords[i];
          pointIdx++;
        }
      }
      
#ifdef __ARM_NEON
      // Use NEON optimized transform on ARM
      transformPoints_NEON(viewMatrix, points, points, 16);
#else
      viewMatrix.mapPoints(points, points, 16);
#endif
      
      // Generate all vertices in one pass with optimized memory layout
      pointIdx = 0;
      for (int i = 0; i < 4; ++i) {
        const float yOffset = yOffsets[i];
        for (int j = 0; j < 4; ++j) {
          const Point& point = points[pointIdx++];
          
          // Position
          vertices[index++] = point.x;
          vertices[index++] = point.y;
          
          // Color
          *reinterpret_cast<uint32_t*>(&vertices[index++]) = packedColor;
          
          // Offsets
          vertices[index++] = xOffsets[j];
          vertices[index++] = yOffset;
          
          // Optional scale
          if (useScale) {
            vertices[index++] = maxRadius;
          }
          
          // Reciprocal radii (same for all vertices in rect)
          vertices[index++] = reciprocalRadii[0];
          vertices[index++] = reciprocalRadii[1];
          vertices[index++] = reciprocalRadii[2];
          vertices[index++] = reciprocalRadii[3];
        }
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
  
  // Cache render target to avoid repeated calls
  auto renderTarget = renderPass->renderTarget();
  auto width = renderTarget->width();
  auto height = renderTarget->height();
  
  // Get context and reuse drawingBuffer to avoid reallocation
  auto context = renderPass->getContext();
  auto drawingBuffer = context->drawingBuffer();
  
  // Create the geometry processor with the cached parameters
  auto gp = EllipseGeometryProcessor::Make(drawingBuffer, width, height,
                                         false, UseScale(context));
  
  // Create pipeline only once and reuse for multiple operations
  auto pipeline = createPipeline(renderPass, std::move(gp));
  
  // Get the number of indices once instead of in the inner loop
  auto numIndicesPerRRect = ResourceProvider::NumIndicesPerRRect();
  auto totalIndices = rectCount * numIndicesPerRRect;
  
  // Bind buffers and scissor clip once
  renderPass->bindProgramAndScissorClip(pipeline.get(), scissorRect());
  renderPass->bindBuffers(indexBuffer, vertexBuffer, vertexBufferOffset);
  
  // Draw all rectangles in a single call for better performance
  renderPass->drawIndexed(PrimitiveType::Triangles, 0, totalIndices);
}
}  // namespace tgfx
