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

#include "GeneratorImage.h"
#include "DecoderImage.h"
#include "gpu/ProxyProvider.h"

namespace tgfx {
std::shared_ptr<Image> GeneratorImage::MakeFrom(std::shared_ptr<ImageGenerator> generator) {
  if (generator == nullptr) {
    return nullptr;
  }
  auto image = std::shared_ptr<GeneratorImage>(
      new GeneratorImage(ResourceKey::NewWeak(), std::move(generator)));
  image->weakThis = image;
  return image;
}

GeneratorImage::GeneratorImage(ResourceKey resourceKey, std::shared_ptr<ImageGenerator> generator)
    : TextureImage(std::move(resourceKey)), generator(std::move(generator)) {
}

std::shared_ptr<Image> GeneratorImage::onMakeDecoded(Context* context, bool tryHardware) const {
  if (context != nullptr) {
    if (context->proxyProvider()->hasResourceProxy(resourceKey) ||
        context->resourceCache()->hasResource(resourceKey)) {
      return nullptr;
    }
  }
  auto decoder = ImageDecoder::MakeFrom(generator, tryHardware, true);
  return DecoderImage::MakeFrom(resourceKey, std::move(decoder));
}

std::shared_ptr<TextureProxy> GeneratorImage::onLockTextureProxy(Context* context,
                                                                 const ResourceKey& key,
                                                                 bool mipmapped,
                                                                 uint32_t renderFlags) const {
  return context->proxyProvider()->createTextureProxy(key, generator, mipmapped, renderFlags);
}
}  // namespace tgfx