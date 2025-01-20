
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

#include "gpu/opengl/GLDevice.h"
#include <Windows.h>

namespace tgfx {
class WGLDevice : public GLDevice {
 public:
  static std::shared_ptr<WGLDevice> Make(void* sharedContext);
  static std::shared_ptr<WGLDevice> Wrap(HGLRC hglrc, HDC hdc, bool externallyOwned);

  bool sharableWith(void* nativeContext) const override;

 protected:
  bool onMakeCurrent() override;
  void onClearCurrent() override;

 private:
  WGLDevice(HGLRC hglrc, HDC hdc);
  ~WGLDevice() override;

  HGLRC hglrc;
  HDC hdc;
  bool externallyOwned;
  HGLRC oldHglrc;
  HDC oldHdc;
};
}  // namespace tgfx