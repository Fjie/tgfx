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

#include "GLEllipseGeometryProcessor.h"

namespace tgfx {
PlacementPtr<EllipseGeometryProcessor> EllipseGeometryProcessor::Make(BlockBuffer* buffer,
                                                                      int width, int height,
                                                                      bool stroke, bool useScale) {
  return buffer->make<GLEllipseGeometryProcessor>(width, height, stroke, useScale);
}

GLEllipseGeometryProcessor::GLEllipseGeometryProcessor(int width, int height, bool stroke,
                                                       bool useScale)
    : EllipseGeometryProcessor(width, height, stroke, useScale) {
}

void GLEllipseGeometryProcessor::emitCode(EmitArgs& args) const {
  auto* vertBuilder = args.vertBuilder;
  auto* varyingHandler = args.varyingHandler;
  auto* uniformHandler = args.uniformHandler;

  // emit attributes
  varyingHandler->emitAttributes(*this);

  // Optimize varying data and computation for better GPU performance
  auto offsetType = useScale ? SLType::Float3 : SLType::Float2;
  auto ellipseOffsets = varyingHandler->addVarying("EllipseOffsets", offsetType);
  vertBuilder->codeAppendf("%s = %s;", ellipseOffsets.vsOut().c_str(),
                         inEllipseOffset.name().c_str());

  // Pack radii into a single varying to reduce interpolation cost
  auto ellipseRadii = varyingHandler->addVarying("EllipseRadii", SLType::Float4);
  vertBuilder->codeAppendf("%s = %s;", ellipseRadii.vsOut().c_str(), inEllipseRadii.name().c_str());

  auto* fragBuilder = args.fragBuilder;
  // setup pass through color
  auto color = varyingHandler->addVarying("Color", SLType::Float4);
  vertBuilder->codeAppendf("%s = %s;", color.vsOut().c_str(), inColor.name().c_str());
  
  // Use direct color assignment for better performance
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), color.fsIn().c_str());

  // Setup position
  args.vertBuilder->emitNormalizedPosition(inPosition.name());
  // emit transforms
  emitTransforms(vertBuilder, varyingHandler, uniformHandler, inPosition.asShaderVar(),
                 args.fpCoordTransformHandler);

  // Highly optimized shader code for better GPU performance
  
  // Get offsets 
  fragBuilder->codeAppendf("vec2 offset = %s.xy;", ellipseOffsets.fsIn().c_str());
  
  // Fast path for non-stroke (filled ellipses, common case for rounded rects)
  if (!stroke) {
    // Use efficient dot product directly
    fragBuilder->codeAppend("float test = dot(offset, offset) - 1.0;");
    
    // Pre-calculate the gradient scale factor based on radiuses
    if (useScale) {
      // Optimize by reducing multiplications and combining constants
      fragBuilder->codeAppendf("vec2 grad = 2.0 * offset * (%s.z * %s.xy);", 
                             ellipseOffsets.fsIn().c_str(),
                             ellipseRadii.fsIn().c_str());
    } else {
      // Optimize by pre-multiplying the 2.0 factor
      fragBuilder->codeAppendf("vec2 grad = 2.0 * offset * %s.xy;", 
                             ellipseRadii.fsIn().c_str());
    }
    
    // Calculate gradient magnitude squared
    fragBuilder->codeAppend("float grad_dot = dot(grad, grad);");
    
    // Use a more efficient branch-free approach based on platform capabilities
    if (args.caps->floatIs32Bits) {
      // Avoid the branch with max operation
      fragBuilder->codeAppend("grad_dot = max(grad_dot, 1.1755e-38);");
      
      // Optimize inverse sqrt calculation
      if (useScale) {
        fragBuilder->codeAppendf("float invlen = %s.z * inversesqrt(grad_dot);",
                               ellipseOffsets.fsIn().c_str());
      } else {
        fragBuilder->codeAppend("float invlen = inversesqrt(grad_dot);");
      }
    } else {
      // Use higher threshold for half-precision
      fragBuilder->codeAppend("grad_dot = max(grad_dot, 1.0e-4);");
      
      if (useScale) {
        fragBuilder->codeAppendf("float invlen = %s.z * inversesqrt(grad_dot);",
                               ellipseOffsets.fsIn().c_str());
      } else {
        fragBuilder->codeAppend("float invlen = inversesqrt(grad_dot);");
      }
    }
    
    // Use an optimized mix operation instead of saturate
    fragBuilder->codeAppend("float edgeAlpha = clamp(0.5 - test * invlen, 0.0, 1.0);");
  } else {
    // Stroke path - optimize by reducing redundant calculations
    fragBuilder->codeAppendf("vec2 scaledOffset = offset * %s.xy;", ellipseRadii.fsIn().c_str());
    fragBuilder->codeAppend("float test = dot(scaledOffset, scaledOffset) - 1.0;");
    
    // Calculate gradient once and reuse
    if (useScale) {
      fragBuilder->codeAppendf("vec2 grad = 2.0 * scaledOffset * %s.z;", 
                             ellipseOffsets.fsIn().c_str());
    } else {
      fragBuilder->codeAppend("vec2 grad = 2.0 * scaledOffset;");
    }
    
    // Optimize gradient calculation
    fragBuilder->codeAppend("float grad_dot = dot(grad, grad);");
    if (args.caps->floatIs32Bits) {
      fragBuilder->codeAppend("grad_dot = max(grad_dot, 1.1755e-38);");
    } else {
      fragBuilder->codeAppend("grad_dot = max(grad_dot, 1.0e-4);");
    }
    
    // Calculate inverse length once
    if (useScale) {
      fragBuilder->codeAppendf("float invlen = %s.z * inversesqrt(grad_dot);",
                             ellipseOffsets.fsIn().c_str());
    } else {
      fragBuilder->codeAppend("float invlen = inversesqrt(grad_dot);");
    }
    
    // Use optimized edge alpha calculation
    fragBuilder->codeAppend("float outerAlpha = clamp(0.5 - test * invlen, 0.0, 1.0);");
    
    // Inner curve calculation - optimize by reducing redundant calculations
    fragBuilder->codeAppendf("vec2 innerOffset = %s.xy * %s.zw;", 
                         ellipseOffsets.fsIn().c_str(),
                         ellipseRadii.fsIn().c_str());
    fragBuilder->codeAppend("float innerTest = dot(innerOffset, innerOffset) - 1.0;");
    
    // Reuse gradient calculation pattern from above
    if (useScale) {
      fragBuilder->codeAppendf("vec2 innerGrad = 2.0 * innerOffset * %s.z;", 
                             ellipseOffsets.fsIn().c_str());
    } else {
      fragBuilder->codeAppend("vec2 innerGrad = 2.0 * innerOffset;");
    }
    
    // Optimize gradient magnitude calculation
    fragBuilder->codeAppend("float innerGradDot = dot(innerGrad, innerGrad);");
    if (!args.caps->floatIs32Bits) {
      fragBuilder->codeAppend("innerGradDot = max(innerGradDot, 1.0e-4);");
    }
    
    // Reuse inverse length calculation pattern
    if (useScale) {
      fragBuilder->codeAppendf("float innerInvLen = %s.z * inversesqrt(innerGradDot);",
                             ellipseOffsets.fsIn().c_str());
    } else {
      fragBuilder->codeAppend("float innerInvLen = inversesqrt(innerGradDot);");
    }
    
    // Optimize inner alpha calculation
    fragBuilder->codeAppend("float innerAlpha = clamp(0.5 + innerTest * innerInvLen, 0.0, 1.0);");
    
    // Use direct multiplication instead of separate operations
    fragBuilder->codeAppend("float edgeAlpha = outerAlpha * innerAlpha;");
  }

  // Direct assignment for better performance
  fragBuilder->codeAppendf("%s = vec4(edgeAlpha);", args.outputCoverage.c_str());
}

void GLEllipseGeometryProcessor::setData(UniformBuffer* uniformBuffer,
                                         FPCoordTransformIter* transformIter) const {
  setTransformDataHelper(Matrix::I(), uniformBuffer, transformIter);
}
}  // namespace tgfx
