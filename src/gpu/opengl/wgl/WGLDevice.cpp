
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

#include "wgl/WGLDevice.h"
#include "core/utils/Log.h"

namespace tgfx {

std::shared_ptr<WGLDevice> WGLDevice::Make(void* sharedContext) {
  HDC sharedHdc = reinterpret_cast<HDC>(sharedContext);
  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;

  HDC hdc = GetDC(NULL);
  int pixelFormat = ChoosePixelFormat(hdc, &pfd);
  if (pixelFormat == 0) {
    LOGE("WGLDevice::Make() ChoosePixelFormat failed");
    return nullptr;
  }

  if (!SetPixelFormat(hdc, pixelFormat, &pfd)) {
    LOGE("WGLDevice::Make() SetPixelFormat failed");
    return nullptr;
  }

  HGLRC hglrc = wglCreateContext(hdc);
  if (hglrc == NULL) {
    LOGE("WGLDevice::Make() wglCreateContext failed");
    return nullptr;
  }

  if (sharedContext) {
    if (!wglShareLists(sharedHdc, hglrc)) {
      LOGE("WGLDevice::Make() wglShareLists failed");
      wglDeleteContext(hglrc);
      return nullptr;
    }
  }

  auto device = std::shared_ptr<WGLDevice>(new WGLDevice(hglrc));
  device->hdc = hdc;
  device->hglrc = hglrc;
  return device;
}

std::shared_ptr<WGLDevice> WGLDevice::Current() {
  HGLRC currentContext = wglGetCurrentContext();
  if (currentContext == NULL) {
    return nullptr;
  }
  auto device = std::make_shared<WGLDevice>(currentContext);
  return device;
}

WGLDevice::WGLDevice(HGLRC context) : GLDevice(reinterpret_cast<void*>(context)), hglrc(context) {
}

WGLDevice::~WGLDevice() {
  if (!externallyOwned) {
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hglrc);
    ReleaseDC(NULL, hdc);
  }
}

bool WGLDevice::sharableWith(void* nativeContext) const {
  return hglrc == reinterpret_cast<HGLRC>(nativeContext);
}

bool WGLDevice::onMakeCurrent() {
  if (wglMakeCurrent(hdc, hglrc)) {
    return true;
  }
  LOGE("WGLDevice::onMakeCurrent() failed with error %d", GetLastError());
  return false;
}

void WGLDevice::onClearCurrent() {
  wglMakeCurrent(NULL, NULL);
}

}  // namespace tgfx