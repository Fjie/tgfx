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

#include "tgfx/core/Matrix.h"
#include <cfloat>
#include "core/utils/MathExtra.h"

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

namespace tgfx {

void Matrix::reset() {
  values[SCALE_X] = values[SCALE_Y] = 1;
  values[SKEW_X] = values[SKEW_Y] = values[TRANS_X] = values[TRANS_Y] = 0;
}

bool operator==(const Matrix& a, const Matrix& b) {
  const float* ma = a.values;
  const float* mb = b.values;
  return ma[0] == mb[0] && ma[1] == mb[1] && ma[2] == mb[2] && ma[3] == mb[3] && ma[4] == mb[4] &&
         ma[5] == mb[5];
}

Matrix operator*(const Matrix& a, const Matrix& b) {
  Matrix result;
  result.setConcat(a, b);
  return result;
}

void Matrix::setAll(float sx, float kx, float tx, float ky, float sy, float ty) {
  values[SCALE_X] = sx;
  values[SKEW_X] = kx;
  values[TRANS_X] = tx;
  values[SKEW_Y] = ky;
  values[SCALE_Y] = sy;
  values[TRANS_Y] = ty;
}

void Matrix::get9(float buffer[9]) const {
  memcpy(buffer, values, 6 * sizeof(float));
  buffer[6] = buffer[7] = 0.0f;
  buffer[8] = 1.0f;
}

void Matrix::setTranslate(float tx, float ty) {
  if ((tx != 0) | (ty != 0)) {
    values[TRANS_X] = tx;
    values[TRANS_Y] = ty;

    values[SCALE_X] = values[SCALE_Y] = 1;
    values[SKEW_X] = values[SKEW_Y] = 0;
  } else {
    this->reset();
  }
}

inline float sdot(float a, float b, float c, float d) {
  return a * b + c * d;
}

void Matrix::preTranslate(float tx, float ty) {
  values[TRANS_X] += sdot(values[SCALE_X], tx, values[SKEW_X], ty);
  values[TRANS_Y] += sdot(values[SKEW_Y], tx, values[SCALE_Y], ty);
}

void Matrix::postTranslate(float tx, float ty) {
  values[TRANS_X] += tx;
  values[TRANS_Y] += ty;
}

void Matrix::setScale(float sx, float sy, float px, float py) {
  if (1 == sx && 1 == sy) {
    this->reset();
  } else {
    values[SCALE_X] = sx;
    values[SKEW_X] = 0;
    values[TRANS_X] = px - sx * px;
    values[SKEW_Y] = 0;
    values[SCALE_Y] = sy;
    values[TRANS_Y] = py - sy * py;
  }
}

void Matrix::setScale(float sx, float sy) {
  if (1 == sx && 1 == sy) {
    this->reset();
  } else {
    values[SCALE_X] = sx;
    values[SCALE_Y] = sy;
    values[TRANS_X] = values[TRANS_Y] = values[SKEW_X] = values[SKEW_Y] = 0;
  }
}

void Matrix::preScale(float sx, float sy, float px, float py) {
  if (1 == sx && 1 == sy) {
    return;
  }
  this->preConcat({sx, 0, px - sx * px, 0, sy, py - sy * py});
}

void Matrix::preScale(float sx, float sy) {
  if (1 == sx && 1 == sy) {
    return;
  }
  values[SCALE_X] *= sx;
  values[SKEW_Y] *= sx;
  values[SKEW_X] *= sy;
  values[SCALE_Y] *= sy;
}

void Matrix::postScale(float sx, float sy, float px, float py) {
  if (1 == sx && 1 == sy) {
    return;
  }
  this->postConcat({sx, 0, px - sx * px, 0, sy, py - sy * py});
}

void Matrix::postScale(float sx, float sy) {
  if (1 == sx && 1 == sy) {
    return;
  }
  this->postConcat({sx, 0, 0, 0, sy, 0});
}

void Matrix::setSinCos(float sinV, float cosV, float px, float py) {
  const float oneMinusCosV = 1 - cosV;
  values[SCALE_X] = cosV;
  values[SKEW_X] = -sinV;
  values[TRANS_X] = sdot(sinV, py, oneMinusCosV, px);
  values[SKEW_Y] = sinV;
  values[SCALE_Y] = cosV;
  values[TRANS_Y] = sdot(-sinV, px, oneMinusCosV, py);
}

void Matrix::setSinCos(float sinV, float cosV) {
  values[SCALE_X] = cosV;
  values[SKEW_X] = -sinV;
  values[TRANS_X] = 0;
  values[SKEW_Y] = sinV;
  values[SCALE_Y] = cosV;
  values[TRANS_Y] = 0;
}

void Matrix::setRotate(float degrees, float px, float py) {
  float rad = DegreesToRadians(degrees);
  this->setSinCos(SinSnapToZero(rad), CosSnapToZero(rad), px, py);
}

void Matrix::setRotate(float degrees) {
  float rad = DegreesToRadians(degrees);
  this->setSinCos(SinSnapToZero(rad), CosSnapToZero(rad));
}

void Matrix::preRotate(float degrees, float px, float py) {
  Matrix m;
  m.setRotate(degrees, px, py);
  this->preConcat(m);
}

void Matrix::preRotate(float degrees) {
  Matrix m;
  m.setRotate(degrees);
  this->preConcat(m);
}

void Matrix::postRotate(float degrees, float px, float py) {
  Matrix m;
  m.setRotate(degrees, px, py);
  this->postConcat(m);
}

void Matrix::postRotate(float degrees) {
  Matrix m;
  m.setRotate(degrees);
  this->postConcat(m);
}

void Matrix::setSkew(float kx, float ky, float px, float py) {
  values[SCALE_X] = 1;
  values[SKEW_X] = kx;
  values[TRANS_X] = -kx * py;
  values[SKEW_Y] = ky;
  values[SCALE_Y] = 1;
  values[TRANS_Y] = -ky * px;
}

void Matrix::setSkew(float kx, float ky) {
  values[SCALE_X] = 1;
  values[SKEW_X] = kx;
  values[TRANS_X] = 0;
  values[SKEW_Y] = ky;
  values[SCALE_Y] = 1;
  values[TRANS_Y] = 0;
}

void Matrix::preSkew(float kx, float ky, float px, float py) {
  this->preConcat({1, kx, -kx * py, ky, 1, -ky * px});
}

void Matrix::preSkew(float kx, float ky) {
  this->preConcat({1, kx, 0, ky, 1, 0});
}

void Matrix::postSkew(float kx, float ky, float px, float py) {
  this->postConcat({1, kx, -kx * py, ky, 1, -ky * px});
}

void Matrix::postSkew(float kx, float ky) {
  this->postConcat({1, kx, 0, ky, 1, 0});
}

void Matrix::setConcat(const Matrix& first, const Matrix& second) {
  const float* a = first.values;
  const float* b = second.values;
  
  // Fast path for identity matrices
  if (first.isIdentity()) {
    if (this != &second) {
      memcpy(values, b, 6 * sizeof(float));
    }
    return;
  }
  
  if (second.isIdentity()) {
    if (this != &first) {
      memcpy(values, a, 6 * sizeof(float));
    }
    return;
  }
  
  // Fast path for translation only
  const bool firstIsTranslateOnly = 
    (a[SCALE_X] == 1.0f && a[SCALE_Y] == 1.0f && a[SKEW_X] == 0.0f && a[SKEW_Y] == 0.0f);
  
  const bool secondIsTranslateOnly = 
    (b[SCALE_X] == 1.0f && b[SCALE_Y] == 1.0f && b[SKEW_X] == 0.0f && b[SKEW_Y] == 0.0f);
    
  if (firstIsTranslateOnly && secondIsTranslateOnly) {
    // Just add translations
    values[SCALE_X] = 1.0f;
    values[SKEW_X] = 0.0f;
    values[TRANS_X] = a[TRANS_X] + b[TRANS_X];
    values[SKEW_Y] = 0.0f;
    values[SCALE_Y] = 1.0f;
    values[TRANS_Y] = a[TRANS_Y] + b[TRANS_Y];
    return;
  }
  
  // Fast paths for other common cases
  if (firstIsTranslateOnly) {
    // First matrix is translation only
    if (this != &second) {
      memcpy(values, b, 6 * sizeof(float));
    }
    values[TRANS_X] += a[TRANS_X];
    values[TRANS_Y] += a[TRANS_Y];
    return;
  }
  
  if (secondIsTranslateOnly) {
    // Second matrix is translation only
    if (this != &first) {
      memcpy(values, a, 6 * sizeof(float));
    }
    values[TRANS_X] += b[TRANS_X];
    values[TRANS_Y] += b[TRANS_Y];
    return;
  }
  
  // General case - fully compute the matrix product
#ifdef __ARM_NEON
  // Load matrix values into NEON registers
  // Matrix A
  float32x2_t a_col0 = vld1_f32(&a[SCALE_X]); // a00, a01
  float32x2_t a_col1 = vld1_f32(&a[SKEW_Y]);  // a10, a11
  float32x2_t a_col2 = vld1_f32(&a[TRANS_X]); // a20, a21
  
  // Matrix B
  float32x2_t b_row0 = vld1_f32(&b[SCALE_X]); // b00, b01
  float32x2_t b_row1 = vld1_f32(&b[SKEW_Y]);  // b10, b11
  float32x2_t b_row2 = vld1_f32(&b[TRANS_X]); // b20, b21
  
  // Use scalar calculations instead of trying complex NEON operations
  // This is simpler and less error-prone
  values[SCALE_X] = a_col0[0] * b_row0[0] + a_col0[1] * b_row1[0];
  values[SKEW_X] = a_col0[0] * b_row0[1] + a_col0[1] * b_row1[1];
  values[TRANS_X] = a_col0[0] * b_row2[0] + a_col0[1] * b_row2[1] + a_col2[0];
  values[SKEW_Y] = a_col1[0] * b_row0[0] + a_col1[1] * b_row1[0];
  values[SCALE_Y] = a_col1[0] * b_row0[1] + a_col1[1] * b_row1[1];
  values[TRANS_Y] = a_col1[0] * b_row2[0] + a_col1[1] * b_row2[1] + a_col2[1];
#else
  const float m00 = a[SCALE_X] * b[SCALE_X] + a[SKEW_X] * b[SKEW_Y];
  const float m01 = a[SCALE_X] * b[SKEW_X] + a[SKEW_X] * b[SCALE_Y];
  const float m02 = a[SCALE_X] * b[TRANS_X] + a[SKEW_X] * b[TRANS_Y] + a[TRANS_X];
  const float m10 = a[SKEW_Y] * b[SCALE_X] + a[SCALE_Y] * b[SKEW_Y];
  const float m11 = a[SKEW_Y] * b[SKEW_X] + a[SCALE_Y] * b[SCALE_Y];
  const float m12 = a[SKEW_Y] * b[TRANS_X] + a[SCALE_Y] * b[TRANS_Y] + a[TRANS_Y];
  
  values[SCALE_X] = m00;
  values[SKEW_X] = m01;
  values[TRANS_X] = m02;
  values[SKEW_Y] = m10;
  values[SCALE_Y] = m11;
  values[TRANS_Y] = m12;
#endif
}

void Matrix::preConcat(const Matrix& matrix) {
  // check for identity first, so we don't do a needless copy of ourselves
  // to ourselves inside setConcat()
  if (!matrix.isIdentity()) {
    this->setConcat(*this, matrix);
  }
}

void Matrix::postConcat(const Matrix& matrix) {
  // check for identity first, so we don't do a needless copy of ourselves
  // to ourselves inside setConcat()
  if (!matrix.isIdentity()) {
    this->setConcat(matrix, *this);
  }
}

bool Matrix::invertible() const {
  // Direct calculation instead of function call 
  const float det = values[SCALE_X] * values[SCALE_Y] - values[SKEW_Y] * values[SKEW_X];
  // Use fixed epsilon value for faster comparison
  const float epsilon = 1e-8f;
  return fabsf(det) >= epsilon;
}

bool Matrix::invertNonIdentity(Matrix* inverse) const {
  const float sx = values[SCALE_X];
  const float kx = values[SKEW_X];
  const float ky = values[SKEW_Y];
  const float sy = values[SCALE_Y];
  const float tx = values[TRANS_X];
  const float ty = values[TRANS_Y];

  // Fast path for scale/translate only (no skew)
  if (ky == 0 && kx == 0) {
    if (sx == 0 || sy == 0) {
      return false;
    }
    // Fast reciprocal calculation
    const float invSx = 1.0f / sx;
    const float invSy = 1.0f / sy;
    inverse->setAll(invSx, 0, -tx * invSx, 0, invSy, -ty * invSy);
    return true;
  }
  
  // General case with skew
  const float det = sx * sy - ky * kx;
  const float epsilon = 1e-8f;
  if (fabsf(det) < epsilon) {
    return false;
  }
  
  // Calculate inverse matrix components
  const float invDet = 1.0f / det;
  const float invsX = sy * invDet;
  const float invkY = -ky * invDet;
  const float invkX = -kx * invDet;
  const float invsY = sx * invDet;
  const float invtX = -(invsX * tx + invkX * ty);
  const float invtY = -(invkY * tx + invsY * ty);
  
  inverse->setAll(invsX, invkX, invtX, invkY, invsY, invtY);
  return true;
}

void Matrix::mapPoints(Point dst[], const Point src[], int count) const {
  if (count <= 0) {
    return;
  }
  
  // Fast path for mapping exactly 4 points (common case for rectangles)
  if (count == 4) {
    // Cache matrix values to minimize member access
    const float sx = values[SCALE_X];
    const float ky = values[SKEW_Y];
    const float kx = values[SKEW_X];
    const float sy = values[SCALE_Y]; 
    const float tx = values[TRANS_X];
    const float ty = values[TRANS_Y];
    
    // Fast path for identity matrix
    if (sx == 1.0f && sy == 1.0f && kx == 0.0f && ky == 0.0f && tx == 0.0f && ty == 0.0f) {
      if (src != dst) {
        memcpy(dst, src, 4 * sizeof(Point));
      }
      return;
    }
    
    // Fast path for translation-only matrix
    if (sx == 1.0f && sy == 1.0f && kx == 0.0f && ky == 0.0f) {
      for (int i = 0; i < 4; i++) {
        dst[i].x = src[i].x + tx;
        dst[i].y = src[i].y + ty;
      }
      return;
    }
    
#ifdef __ARM_NEON
    // Use NEON SIMD for mapping exactly 4 points when possible
    // Load matrix values into NEON registers - only needed if we want to access them differently
    
    // Process each point directly - for 4 points this approach is faster than extra SIMD setup
    const float x0 = src[0].x;
    const float y0 = src[0].y;
    dst[0].x = x0 * sx + y0 * kx + tx;
    dst[0].y = x0 * ky + y0 * sy + ty;
    
    const float x1 = src[1].x;
    const float y1 = src[1].y;
    dst[1].x = x1 * sx + y1 * kx + tx;
    dst[1].y = x1 * ky + y1 * sy + ty;
    
    const float x2 = src[2].x;
    const float y2 = src[2].y;
    dst[2].x = x2 * sx + y2 * kx + tx;
    dst[2].y = x2 * ky + y2 * sy + ty;
    
    const float x3 = src[3].x;
    const float y3 = src[3].y;
    dst[3].x = x3 * sx + y3 * kx + tx;
    dst[3].y = x3 * ky + y3 * sy + ty;
    
    return;
#else
    // Optimized scalar implementation for 4 points
    for (int i = 0; i < 4; i++) {
      const float srcX = src[i].x;
      const float srcY = src[i].y;
      dst[i].x = srcX * sx + srcY * kx + tx;
      dst[i].y = srcX * ky + srcY * sy + ty;
    }
    return;
#endif
  }
  
  // Cache matrix values to minimize member access in the loop
  const float sx = values[SCALE_X];
  const float ky = values[SKEW_Y];
  const float kx = values[SKEW_X];
  const float sy = values[SCALE_Y]; 
  const float tx = values[TRANS_X];
  const float ty = values[TRANS_Y];
  
  // Fast path for identity matrix
  if (sx == 1.0f && sy == 1.0f && kx == 0.0f && ky == 0.0f && tx == 0.0f && ty == 0.0f) {
    if (src != dst) {
      memcpy(dst, src, static_cast<size_t>(count) * sizeof(Point));
    }
    return;
  }
  
  // Fast path for translation-only matrix
  if (sx == 1.0f && sy == 1.0f && kx == 0.0f && ky == 0.0f) {
#ifdef __ARM_NEON
    if (count >= 4) {
      // Process batches of 4 points with NEON SIMD
      int i = 0;
      for (; i <= count - 4; i += 4) {
        // Load 4 points (8 floats)
        float32x4x2_t points = vld2q_f32(reinterpret_cast<const float*>(&src[i]));
        
        // Add translation to each coordinate
        points.val[0] = vaddq_f32(points.val[0], vdupq_n_f32(tx));
        points.val[1] = vaddq_f32(points.val[1], vdupq_n_f32(ty));
        
        // Store 4 transformed points
        vst2q_f32(reinterpret_cast<float*>(&dst[i]), points);
      }
      
      // Handle remaining points
      for (; i < count; i++) {
        dst[i].x = src[i].x + tx;
        dst[i].y = src[i].y + ty;
      }
      return;
    }
#endif
    // Non-SIMD fallback for translation
    for (int i = 0; i < count; i++) {
      dst[i].x = src[i].x + tx;
      dst[i].y = src[i].y + ty;
    }
    return;
  }
  
  // Fast path for scale-only matrix (no skew)
  if (kx == 0.0f && ky == 0.0f) {
#ifdef __ARM_NEON
    if (count >= 4) {
      // Process batches of 4 points with NEON SIMD
      int i = 0;
      for (; i <= count - 4; i += 4) {
        // Load 4 points (8 floats)
        float32x4x2_t points = vld2q_f32(reinterpret_cast<const float*>(&src[i]));
        
        // Scale X and Y coordinates
        points.val[0] = vmulq_n_f32(points.val[0], sx);
        points.val[1] = vmulq_n_f32(points.val[1], sy);
        
        // Add translation
        points.val[0] = vaddq_f32(points.val[0], vdupq_n_f32(tx));
        points.val[1] = vaddq_f32(points.val[1], vdupq_n_f32(ty));
        
        // Store 4 transformed points
        vst2q_f32(reinterpret_cast<float*>(&dst[i]), points);
      }
      
      // Handle remaining points
      for (; i < count; i++) {
        const float x = src[i].x;
        const float y = src[i].y;
        dst[i].x = x * sx + tx;
        dst[i].y = y * sy + ty;
      }
      return;
    }
#endif
    // Non-SIMD fallback for scale+translate
    for (int i = 0; i < count; i++) {
      const float x = src[i].x;
      const float y = src[i].y;
      dst[i].x = x * sx + tx;
      dst[i].y = y * sy + ty;
    }
    return;
  }
  
  // General case with skew
#ifdef __ARM_NEON
  if (count >= 4) {
    int i = 0;
    float32x4_t vSx = vdupq_n_f32(sx);
    float32x4_t vKx = vdupq_n_f32(kx);
    float32x4_t vKy = vdupq_n_f32(ky);
    float32x4_t vSy = vdupq_n_f32(sy);
    float32x4_t vTx = vdupq_n_f32(tx);
    float32x4_t vTy = vdupq_n_f32(ty);
    
    for (; i <= count - 4; i += 4) {
      // Load 4 x-coordinates
      float32x4_t vX = vld1q_f32(&reinterpret_cast<const float*>(&src[i])[0]);
      // Load 4 y-coordinates with a stride of 2
      float32x4_t vY = vld1q_f32(&reinterpret_cast<const float*>(&src[i])[1]);
      
      // Transform x-coordinates: x * sx + y * kx + tx
      float32x4_t resX = vmlaq_f32(vTx, vX, vSx);
      resX = vmlaq_f32(resX, vY, vKx);
      
      // Transform y-coordinates: x * ky + y * sy + ty
      float32x4_t resY = vmlaq_f32(vTy, vX, vKy);
      resY = vmlaq_f32(resY, vY, vSy);
      
      // Store x-coordinates with stride of 2
      vst1q_f32(&reinterpret_cast<float*>(&dst[i])[0], resX);
      // Store y-coordinates with stride of 2
      vst1q_f32(&reinterpret_cast<float*>(&dst[i])[1], resY);
    }
    
    // Handle remaining points
    for (; i < count; i++) {
      const float x = src[i].x;
      const float y = src[i].y;
      dst[i].x = x * sx + y * kx + tx;
      dst[i].y = x * ky + y * sy + ty;
    }
    return;
  }
#endif

  // Non-SIMD fallback
  for (int i = 0; i < count; i++) {
    const float x = src[i].x;
    const float y = src[i].y;
    dst[i].x = x * sx + y * kx + tx;
    dst[i].y = x * ky + y * sy + ty;
  }
}

void Matrix::mapXY(float x, float y, Point* result) const {
  auto tx = values[TRANS_X];
  auto ty = values[TRANS_Y];
  auto sx = values[SCALE_X];
  auto sy = values[SCALE_Y];
  auto kx = values[SKEW_X];
  auto ky = values[SKEW_Y];
  result->set(x * sx + y * kx + tx, x * ky + y * sy + ty);
}

bool Matrix::rectStaysRect() const {
  float sx = values[SCALE_X];
  float kx = values[SKEW_X];
  float ky = values[SKEW_Y];
  float sy = values[SCALE_Y];
  if (kx != 0 || ky != 0) {
    return sx == 0 && sy == 0 && ky != 0 && kx != 0;
  }
  return sx != 0 && sy != 0;
}

void Matrix::mapRect(Rect* dst, const Rect& src) const {
  Point quad[4];
  quad[0].set(src.left, src.top);
  quad[1].set(src.right, src.top);
  quad[2].set(src.right, src.bottom);
  quad[3].set(src.left, src.bottom);
  mapPoints(quad, quad, 4);
  dst->setBounds(quad, 4);
}

float Matrix::getMinScale() const {
  float results[2];
  if (getMinMaxScaleFactors(results)) {
    return results[0];
  }
  return -1.0f;
}

float Matrix::getMaxScale() const {
  float results[2];
  if (getMinMaxScaleFactors(results)) {
    return results[1];
  }
  return -1.0f;
}

Point Matrix::getAxisScales() const {
  Point scale = {};
  double sx = values[SCALE_X];
  double kx = values[SKEW_X];
  double ky = values[SKEW_Y];
  double sy = values[SCALE_Y];
  scale.x = static_cast<float>(sqrt(sx * sx + ky * ky));
  scale.y = static_cast<float>(sqrt(kx * kx + sy * sy));
  return scale;
}

bool Matrix::getMinMaxScaleFactors(float* results) const {
  float a = sdot(values[SCALE_X], values[SCALE_X], values[SKEW_Y], values[SKEW_Y]);
  float b = sdot(values[SCALE_X], values[SKEW_X], values[SCALE_Y], values[SKEW_Y]);
  float c = sdot(values[SKEW_X], values[SKEW_X], values[SCALE_Y], values[SCALE_Y]);
  float bSqd = b * b;
  if (bSqd <= FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO) {
    results[0] = a;
    results[1] = c;
    if (results[0] > results[1]) {
      using std::swap;
      swap(results[0], results[1]);
    }
  } else {
    float aminusc = a - c;
    float apluscdiv2 = (a + c) * 0.5f;
    float x = sqrtf(aminusc * aminusc + 4 * bSqd) * 0.5f;
    results[0] = apluscdiv2 - x;
    results[1] = apluscdiv2 + x;
  }
  auto isFinite = (results[0] * 0 == 0);
  if (!isFinite) {
    return false;
  }
  if (results[0] < 0) {
    results[0] = 0;
  }
  results[0] = sqrtf(results[0]);
  isFinite = (results[1] * 0 == 0);
  if (!isFinite) {
    return false;
  }
  if (results[1] < 0) {
    results[1] = 0;
  }
  results[1] = sqrtf(results[1]);
  return true;
}

bool Matrix::hasNonIdentityScale() const {
  double sx = values[SCALE_X];
  double ky = values[SKEW_Y];
  if (sqrt(sx * sx + ky * ky) != 1.0) {
    return true;
  }
  double kx = values[SKEW_X];
  double sy = values[SCALE_Y];
  if (sqrt(kx * kx + sy * sy) != 1.0) {
    return true;
  }
  return false;
}

bool Matrix::isTranslate() const {
  return values[SCALE_X] == 1 && values[SCALE_Y] == 1 && values[SKEW_X] == 0 && values[SKEW_Y] == 0;
}

bool Matrix::isFinite() const {
  return FloatsAreFinite(values, 6);
}

const Matrix& Matrix::I() {
  static const Matrix identity = Matrix::MakeAll(1, 0, 0, 0, 1, 0);
  return identity;
}
}  // namespace tgfx
