/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderExternalImage.h"

#include "GLContextProvider.h"
#include "GLTextureImage.h"

namespace mozilla {
namespace layers {

WRExternalImage GetExternalImage(void* aObj, WRExternalImageId aId)
{
  MOZ_ASSERT(aObj);
  WebRenderExternalImage* obj = static_cast<WebRenderExternalImage*>(aObj);
  gl::TextureImage* image = obj->GetExternalImage(aId);

  if (image) {
    printf_stderr("bignose gecko get_img:%p (%d,%d) id:%lld tex:%d\n",
        obj, image->GetSize().width, image->GetSize().height,
        aId.id, image->GetTextureID());
    return WRExternalImage {
      TEXTURE_HANDLE,
      0.0f, 0.0f,
      static_cast<float>(image->GetSize().width), static_cast<float>(image->GetSize().height),
      image->GetTextureID()
    };
  }

  printf_stderr("bignose gecko ERROR get_img:%p id:%lld\n",
      obj, aId.id);

  return WRExternalImage { TEXTURE_HANDLE, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
}

void ReleaseExternalImage(void* aObj, WRExternalImageId aId)
{
  MOZ_ASSERT(aObj);
  WebRenderExternalImage* obj = static_cast<WebRenderExternalImage*>(aObj);
  //obj->RemoveExternalImage(aId);
  obj->ReleaseExternalImage(aId);

  printf_stderr("bignose gecko release_img:%p id:%lld\n", obj, aId.id);
}

WebRenderExternalImage::WebRenderExternalImage(gl::GLContext* aGlContext)
  : mCurrentImageId(0)
  , mGLContext(aGlContext)
{
  printf_stderr("bignose WebRenderExternalImage ctor\n");
  //NS_ERROR("bignose WebRenderExternalImage ctor");
}

WebRenderExternalImage::~WebRenderExternalImage()
{
}

bool
WebRenderExternalImage::AddExternalImage(gfx::DataSourceSurface* aSurf,
                                         WRExternalImageId& aId)
{
  uint64_t id = GetNextImageId();
  printf_stderr("bignose gecko AddExternalImage:%p for id:%lld begin\n", this, id);
  RefPtr<gl::TextureImage> texture =
      gl::CreateBasicTextureImage(mGLContext,
                                  aSurf->GetSize(),
                                  gl::TextureImage::ContentType::COLOR_ALPHA,
                                  LOCAL_GL_CLAMP_TO_EDGE,
                                  gl::TextureImage::UseNearestFilter);
  if (texture) {
    // upload data to textureImage
    texture->UpdateFromDataSource(aSurf);

    //uint64_t id = GetNextImageId();
    mImages.insert(std::make_pair(id, texture));
    aId.id = id;

    printf_stderr("bignose gecko insert texture:%d for id:%lld\n",
        texture->GetTextureID(), id);
    //return true;
  } else {
    printf_stderr("bignose gecko insert failed for id:%lld\n", id);
  }

  printf_stderr("bignose gecko AddExternalImage:%p for id:%lld end\n", this, id);

  if (texture)
    return true;
  return false;
}

WRExternalImageHandler
WebRenderExternalImage::GetExternalImageHandler()
{
  return WRExternalImageHandler {
    this,
    mozilla::layers::GetExternalImage,
    mozilla::layers::ReleaseExternalImage,
  };
}

WRImageIdType
WebRenderExternalImage::GetNextImageId()
{
  return ++mCurrentImageId;
}

gl::TextureImage*
WebRenderExternalImage::GetExternalImage(const WRExternalImageId& aId)
{
  ImageMap::iterator image = mImages.find(aId.id);

  if (image != mImages.end()) {
    return image->second;
  }

  return nullptr;
}

bool
WebRenderExternalImage::ReleaseExternalImage(const WRExternalImageId& aId)
{
  printf_stderr("bignose WebRenderExternalImage::ReleaseExternalImage %p, id:%lld, no op\n", this, aId.id);
  return true;
}

bool
WebRenderExternalImage::RemoveExternalImage(const WRExternalImageId& aId)
{
  ImageMap::iterator image = mImages.find(aId.id);

  if (image != mImages.end()) {
    mImages.erase(image);
    return true;
  }

  return false;
  //return true;
}

} // namespace layers
} // namespace mozilla
