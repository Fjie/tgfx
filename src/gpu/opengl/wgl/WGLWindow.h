
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

#pragma once

#include "gpu/opengl/GLWindow.h"
#include <windows.h>

namespace tgfx {
class WGLWindow : public GLWindow {
 public:
  static std::shared_ptr<WGLWindow> MakeFrom(HWND hwnd, HGLRC sharedContext);

 protected:
  bool onMakeCurrent() override;
  void onClearCurrent() override;

 private:
  WGLWindow(HWND hwnd, HDC hdc, HGLRC context);
  ~WGLWindow() override;

  HWND hwnd;
  HDC hdc;
  HGLRC hglrc;
};
}  // namespace tgfx