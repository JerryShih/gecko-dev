/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderExternalImage_h
#define mozilla_layers_WebRenderExternalImage_h

#include <vector>
#include "mozilla/RefPtr.h"

#include "mozilla/gfx/webrender.h"

namespace mozilla {

namespace gl {
class GLContext;
class TextureImage;
}

namespace layers {

class WebRenderExternalImage final
{
  NS_INLINE_DECL_REFCOUNTING(WebRenderExternalImage)

  friend WRExternalImage GetExternalImage(void*, WRExternalImageId);
  friend void ReleaseExternalImage(void*, WRExternalImageId);

public:
  WebRenderExternalImage(gl::GLContext* aGlContext);

  // The external image add/release interface for WR.
  WRExternalImageHandler GetExternalImageHandler();

  // Create a new WR image for a DataSourceSurface and return a corresponding
  // WRExternalImageKey to it. Return false if WR image creation is failed.
  bool AddExternalImage(gfx::DataSourceSurface* aSurf, WRExternalImageId& aKey);


private:
  ~WebRenderExternalImage();
public:
  WRImageIdType GetNextImageId();

  gl::TextureImage* GetExternalImage(const WRExternalImageId& aKey);
  bool ReleaseExternalImage(const WRExternalImageId& aKey);
  bool RemoveExternalImage(const WRExternalImageId& aKey);

  typedef std::map<WRImageIdType, RefPtr<gl::TextureImage>> ImageMap;
  ImageMap mImages;

  WRImageIdType mCurrentImageId;

  RefPtr<gl::GLContext> mGLContext;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderExternalImage_h
