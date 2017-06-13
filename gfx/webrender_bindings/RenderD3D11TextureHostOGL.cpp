/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderD3D11TextureHostOGL.h"

#include "GLLibraryEGL.h"
#include "GLContextEGL.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace wr {

RenderDXGITextureHostOGL::RenderDXGITextureHostOGL(WindowsHandle aHandle,
                                                   gfx::SurfaceFormat aFormat,
                                                   gfx::IntSize aSize)
  : mHandle(aHandle)
  , mTextureHandle{ 0, 0 }
  , mSurface(0)
  , mStream(0)
  , mFormat(aFormat)
  , mSize(aSize)
{
  MOZ_COUNT_CTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
}

RenderDXGITextureHostOGL::~RenderDXGITextureHostOGL()
{
  MOZ_COUNT_DTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
  DeleteTextureHandle();
}

void
RenderDXGITextureHostOGL::SetGLContext(gl::GLContext* aContext)
{
  if (mGL.get() != aContext) {
    // release the texture handle in the previous gl context
    DeleteTextureHandle();
    mGL = aContext;
  }
}

bool
RenderDXGITextureHostOGL::Lock()
{
  if (mTextureHandle[0]) {
    return true;
  }

  gl::GLLibraryEGL* egl = &gl::sEGLLibrary;

  if (mFormat != gfx::SurfaceFormat::NV12) {
    EGLint pbuffer_attributes[] = {
        LOCAL_EGL_WIDTH, mSize.width,
        LOCAL_EGL_HEIGHT, mSize.height,
        LOCAL_EGL_TEXTURE_TARGET, LOCAL_EGL_TEXTURE_2D,
        // XXX: EGL only support RGBA or RGB.
        LOCAL_EGL_TEXTURE_FORMAT, LOCAL_EGL_TEXTURE_RGBA,
        LOCAL_EGL_MIPMAP_TEXTURE, LOCAL_EGL_FALSE,
        LOCAL_EGL_NONE
    };
    mSurface = egl->fCreatePbufferFromClientBuffer(egl->Display(),
                                                   LOCAL_EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                                   reinterpret_cast<EGLClientBuffer>(mHandle),
                                                   gl::GLContextEGL::Cast(mGL.get())->mConfig,
                                                   pbuffer_attributes);
    void* keyedMutex = nullptr;
    egl->fQuerySurfacePointerANGLE(egl->Display(), mSurface, LOCAL_EGL_DXGI_KEYED_MUTEX_ANGLE, &keyedMutex);
    mKeyedMutex = static_cast<IDXGIKeyedMutex*>(keyedMutex);

    mGL->fGenTextures(1, &mTextureHandle[0]);
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle[0]);
    mGL->fTexParameterf(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameterf(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
    egl->fBindTexImage(egl->Display(), mSurface, LOCAL_EGL_BACK_BUFFER);

    if (mKeyedMutex) {
      mKeyedMutex->AcquireSync(0, 10000);
    }
  } else {
    // Fetch the D3D11 device.
    EGLDeviceEXT eglDevice = nullptr;
    egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
    MOZ_ASSERT(eglDevice);
    ID3D11Device* device = nullptr;
    egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE, (EGLAttrib*)&device);
    MOZ_ASSERT(device);

    // Create the NV12 D3D11 texture.
    if (FAILED(device->OpenSharedResource((HANDLE)mHandle,
                                          __uuidof(ID3D11Texture2D),
                                          (void**)(ID3D11Texture2D**)getter_AddRefs(mTexture)))) {
      NS_WARNING("Failed to open shared texture");
      return false;
    }

    // Create the stream.
    const EGLint streamAttributes[] = {
        LOCAL_EGL_CONSUMER_LATENCY_USEC_KHR, 0,
        LOCAL_EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0,
        LOCAL_EGL_NONE,
    };
    mStream = egl->fCreateStreamKHR(egl->Display(), streamAttributes);
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);
    MOZ_ASSERT(mStream);

    // Setup the NV12 stream consumer/producer.
    EGLAttrib consumerAttributes[] = {
        LOCAL_EGL_COLOR_BUFFER_TYPE,
        LOCAL_EGL_YUV_BUFFER_EXT,
        LOCAL_EGL_YUV_NUMBER_OF_PLANES_EXT,
        2,
        LOCAL_EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        0,
        LOCAL_EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
        1,
        LOCAL_EGL_NONE,
    };
    mGL->fGenTextures(2, mTextureHandle);
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureHandle[0]);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL_OES, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    mGL->fActiveTexture(LOCAL_GL_TEXTURE1);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureHandle[1]);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL_OES, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    MOZ_ASSERT(egl->fStreamConsumerGLTextureExternalAttribsNV(egl->Display(), mStream, consumerAttributes));
    EGLAttrib producerAttributes[] = {
        LOCAL_EGL_NONE,
    };
    MOZ_ASSERT(egl->fCreateStreamProducerD3DTextureNV12ANGLE(egl->Display(), mStream, producerAttributes));

    // Insert the NV12 texture.
    EGLAttrib frameAttributes[] = {
        LOCAL_EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, 0,
        LOCAL_EGL_NONE,
    };
    MOZ_ASSERT(egl->fStreamPostD3DTextureNV12ANGLE(egl->Display(), mStream, (void*)mTexture.get(), frameAttributes));

    // Now, we could check the stream status and use the NV12 gl handle.
    EGLint state;
    egl->fQueryStreamKHR(egl->Display(), mStream, LOCAL_EGL_STREAM_STATE_KHR, &state);
    MOZ_ASSERT(LOCAL_EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR == state);
    egl->fStreamConsumerAcquireKHR(egl->Display(), mStream);
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);
  }

  return true;
}

void
RenderDXGITextureHostOGL::Unlock()
{
  if (mTextureHandle[0]) {
    if (mKeyedMutex) {
      mKeyedMutex->ReleaseSync(0);
    }
  }
}

void
RenderDXGITextureHostOGL::DeleteTextureHandle()
{
  if (mTextureHandle[0] != 0 && mGL && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(2, mTextureHandle);
    for(int i = 0; i < 2; ++i) {
      mTextureHandle[i] = 0;
    }

    gl::GLLibraryEGL* egl = &gl::sEGLLibrary;
    if (mSurface) {
      egl->fDestroySurface(egl->Display(), mSurface);
      mSurface = 0;
    }

    if (mStream) {
      egl->fStreamConsumerReleaseKHR(egl->Display(), mStream);
      mStream = 0;
    }

    mTexture = nullptr;
    mKeyedMutex = nullptr;
  }
}

GLuint
RenderDXGITextureHostOGL::GetGLHandle(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::NV12 || aChannelIndex < 2);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::NV12 || aChannelIndex < 1);

  return mTextureHandle[aChannelIndex];
}

gfx::IntSize
RenderDXGITextureHostOGL::GetSize(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::NV12 || aChannelIndex < 2);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::NV12 || aChannelIndex < 1);

  if (aChannelIndex == 0) {
    return mSize;
  } else {
    // The CbCr channel in NV12 format.
    return mSize / 2;
  }
}

} // namespace wr
} // namespace mozilla
