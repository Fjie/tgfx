
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

#include "gpu/opengl/GLProcGetter.h"

#if defined(_WIN32)
#include "gpu/opengl/wgl/WGLProcGetter.h"
#else
#include "gpu/opengl/egl/EGLProcGetter.h"
#endif

namespace tgfx {

std::unique_ptr<GLProcGetter> GLProcGetter::Make() {
  #if defined(_WIN32)
    return std::make_unique<WGLProcGetter>();
  #else
    return std::make_unique<EGLProcGetter>();
  #endif
}

}  // namespace tgfx