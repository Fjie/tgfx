
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

#include "wgl/WGLWindow.h"
#include "core/utils/Log.h"

namespace tgfx {

std::shared_ptr<WGLWindow> WGLWindow::MakeFrom(HWND hwnd, HGLRC sharedContext) {
  HDC hdc = GetDC(hwnd);
  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;

  int pixelFormat = ChoosePixelFormat(hdc, &pfd);
  if (pixelFormat == 0) {
    LOGE("WGLWindow::MakeFrom() ChoosePixelFormat failed");
    return nullptr;
  }

  if (!SetPixelFormat(hdc, pixelFormat, &pfd)) {
    LOGE("WGLWindow::MakeFrom() SetPixelFormat failed");
    return nullptr;
  }

  HGLRC hglrc = wglCreateContext(hdc);
  if (hglrc == NULL) {
    LOGE("WGLWindow::MakeFrom() wglCreateContext failed");
    return nullptr;
  }

  if (sharedContext) {
    if (!wglShareLists(sharedContext, hglrc)) {
      LOGE("WGLWindow::MakeFrom() wglShareLists failed");
      wglDeleteContext(hglrc);
      return nullptr;
    }
  }

  auto window = std::make_shared<WGLWindow>(hwnd, hdc, hglrc);
  return window;
}

WGLWindow::WGLWindow(HWND hwnd, HDC hdc, HGLRC context)
    : GLWindow(reinterpret_cast<void*>(context)), hwnd(hwnd), hdc(hdc), hglrc(context) {
}

WGLWindow::~WGLWindow() {
  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(hglrc);
  ReleaseDC(hwnd, hdc);
}

bool WGLWindow::onMakeCurrent() {
  if (wglMakeCurrent(hdc, hglrc)) {
    return true;
  }
  LOGE("WGLWindow::onMakeCurrent() failed with error %d", GetLastError());
  return false;
}

void WGLWindow::onClearCurrent() {
  wglMakeCurrent(NULL, NULL);
}

}  // namespace tgfx