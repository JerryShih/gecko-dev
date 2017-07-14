/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererOGL.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderBufferTextureHost.h"
#include "mozilla/webrender/RenderTextureHostOGL.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef XP_WIN
#include <d3d11.h>
#include "GLLibraryEGL.h"
#include "GLContextEGL.h"
#endif

namespace mozilla {
namespace wr {

void TestAngle(gl::GLContext* gl)
{
#ifdef XP_WIN
  if (!gl)
      return;

  static const size_t rgbatest_width = 128;
  static const size_t rgbatest_height = 128;
  static unsigned char rgbatest_data[rgbatest_width * rgbatest_height * 4];
  static bool init = false;
  if (!init) {
    init = true;
    int pos = 0;
    for (int i = 0; i < rgbatest_height; ++i) {
      for (int j = 0; j < rgbatest_width; ++j) {
        rgbatest_data[pos++] = 255; //r
        rgbatest_data[pos++] = 0;
        rgbatest_data[pos++] = 255;
        rgbatest_data[pos++] = 255;
      }
    }
  }

  gl::GLLibraryEGL* egl = &gl::sEGLLibrary;

  HANDLE mHandle = 0;
  gfx::IntSize mSize(rgbatest_width, rgbatest_height);
  RefPtr<ID3D11Texture2D> mTexture;

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE, (EGLAttrib*)&device);
  MOZ_ASSERT(device);

  HRESULT res;
  D3D11_TEXTURE2D_DESC desc;
  desc.Width              = rgbatest_width;
  desc.Height             = rgbatest_height;
  desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.MipLevels          = 1;
  desc.ArraySize          = 1;
  desc.SampleDesc.Count   = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage              = D3D11_USAGE_DEFAULT;
  desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags     = 0;
  desc.MiscFlags          = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  D3D11_SUBRESOURCE_DATA subres;
  subres.pSysMem          = rgbatest_data;
  subres.SysMemPitch      = rgbatest_width;
  subres.SysMemSlicePitch = rgbatest_width * rgbatest_height * 4;
  res                      = device->CreateTexture2D(&desc, &subres, (ID3D11Texture2D**)getter_AddRefs(mTexture));

  IDXGIResource* resource = nullptr;
  mTexture->QueryInterface(&resource);
  resource->GetSharedHandle((HANDLE*)&mHandle);


  static const GLfloat squareVertices[] = {
      0.5f, 0.5f,
      -0.5f, 0.5f,
      -0.5f, -0.5f,
	  0.5f, 0.5f,
      -0.5f, -0.5f,
	  0.5f, -0.5f,
  };

  static const GLfloat textureVertices[] = {
      1.0f, 1.0f,
      0.0f, 1.0f,
      0.0f, 0.0f,
      1.0f, 1.0f,
      0.0f,  0.0f,
      1.0f,  0.0f,
  };

  gl->MakeCurrent();
  GLuint vertexbuffer;
  gl->fGenBuffers(1, &vertexbuffer);
  gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, vertexbuffer);
  gl->fBufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, LOCAL_GL_STATIC_DRAW);
  gl->fEnableVertexAttribArray(0);
  gl->fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, 0, 0, 0);

  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  GLuint texbuffer;
  gl->fGenBuffers(1, &texbuffer);
  gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, texbuffer);
  gl->fBufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(textureVertices), textureVertices, LOCAL_GL_STATIC_DRAW);
  gl->fEnableVertexAttribArray(1);
  gl->fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, 0, 0, 0);

  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  // Creating shader
  GLuint vs = gl->fCreateShader(LOCAL_GL_VERTEX_SHADER);
  GLuint ps = gl->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);

  const GLchar* vs_str =
      "attribute vec4 vPosition; \
   attribute vec2 texCoord0; \
   varying vec2 texCoord; \
   void main() { \
     gl_Position = vPosition; \
     texCoord = texCoord0; \
   }";
  const GLchar* ps_str =
      "precision mediump float; \
   uniform sampler2D tex; \
   varying vec2 texCoord; \
   void main() \
   { \
     gl_FragColor = texture2D(tex, texCoord); \
   }";
  //const GLchar* ps_str =
	 // "precision mediump float; \
  // uniform sampler2D tex; \
  // varying vec2 texCoord; \
  // void main() \
  // { \
  //   gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0); \
  // }";

  gl->fShaderSource(vs, 1, &vs_str, NULL);
  gl->fCompileShader(vs);

  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  gl->fShaderSource(ps, 1, &ps_str, NULL);
  gl->fCompileShader(ps);

  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  GLuint program = gl->fCreateProgram();
  gl->fAttachShader(program, vs);
  gl->fAttachShader(program, ps);
  gl->fLinkProgram(program);

  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  EGLConfig config = gl::GLContextEGL::Cast(gl)->mConfig;
  EGLint pbuffer_attributes[] =
  {
      LOCAL_EGL_WIDTH, mSize.width,
      LOCAL_EGL_HEIGHT, mSize.height,
      LOCAL_EGL_TEXTURE_TARGET, LOCAL_EGL_TEXTURE_2D,
      LOCAL_EGL_TEXTURE_FORMAT, LOCAL_EGL_TEXTURE_RGBA,
      LOCAL_EGL_MIPMAP_TEXTURE, LOCAL_EGL_TRUE,
      LOCAL_EGL_NONE
  };

  printf_stderr("bignose mHandle:%d\n", mHandle);
  EGLSurface surface =
      egl->fCreatePbufferFromClientBuffer(egl->Display(),
          LOCAL_EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
          reinterpret_cast<EGLClientBuffer>(mHandle),
          config,
          pbuffer_attributes);

  printf_stderr("bignose line:%d, egl error:%d\n", __LINE__, egl->fGetError());

  void* opaqueKeyedMutex = nullptr;
  egl->fQuerySurfacePointerANGLE(egl->Display(), surface, LOCAL_EGL_DXGI_KEYED_MUTEX_ANGLE, &opaqueKeyedMutex);

  printf_stderr("bignose line:%d, egl error:%d\n", __LINE__, egl->fGetError());

  RefPtr<IDXGIKeyedMutex> keyedMutex = static_cast<IDXGIKeyedMutex*>(opaqueKeyedMutex);
  GLuint tex;
  gl->fGenTextures(1, &tex);
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, tex);

  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  gl->fTexParameterf(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);
  gl->fTexParameterf(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
  gl->fTexParameterf(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameterf(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  // Bind pbuffer and render it.
  egl->fBindTexImage(egl->Display(), surface, LOCAL_EGL_BACK_BUFFER);


  gl->fUseProgram(program);
  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  gl->fBindAttribLocation(program, 0, "vPosition");
  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());
  gl->fBindAttribLocation(program, 1, "texCoord0");
  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());
  GLuint sampler = gl->fGetUniformLocation(program, "tex");
  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());
  gl->fUniform1i(sampler, 0);
  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, tex);
  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());
  //gl->fDepthFunc(LOCAL_GL_ALWAYS);

  gl->fClearColor(0.0, 0.0, 1.0, 1.0);
  gl->fClear(LOCAL_GL_DEPTH_BUFFER_BIT);
  //gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
  //gl->fDisable(LOCAL_GL_DEPTH_TEST);
  //gl->fDisable(LOCAL_GL_SCISSOR_TEST);
  //gl->fDisable(LOCAL_GL_STENCIL_TEST);
  if (keyedMutex) {
	  keyedMutex->AcquireSync(0, 10000);
  }
  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  gl->fDrawArrays(LOCAL_GL_TRIANGLES, 0, 6);
  
  printf_stderr("bignose line:%d, error:%d\n", __LINE__, gl->fGetError());

  if (keyedMutex) {
	  keyedMutex->ReleaseSync(0);
  }
  //gl->fEnable(LOCAL_GL_DEPTH_TEST);
  //gl->fEnable(LOCAL_GL_SCISSOR_TEST);
  //gl->fEnable(LOCAL_GL_STENCIL_TEST);

  egl->fReleaseTexImage(egl->Display(), surface, LOCAL_EGL_BACK_BUFFER);
  //gl->fDisableVertexAttribArray(2);
  //gl->fDisableVertexAttribArray(3);
  //gl->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);

  /*if (!widget)
      return;

  LayoutDeviceIntSize size = widget->GetClientSize();
  IntSize size2(size.width, size.height);
  RefPtr<DataSourceSurface> surf;
  surf = Factory::CreateDataSourceSurfaceWithStride(size2,
      SurfaceFormat::B8G8R8A8,
      size2.width * 4);

  //gl->fClearColor(0.0, 1.0, 0.0, 1.0);
  //gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
  gl::ReadPixelsIntoDataSurface(gl, surf);
  char buffer[256];
  static int i = 0;
  sprintf(buffer, "png/%x%d.png", this, ++i);
  gfxUtils::WriteAsPNG(surf, buffer);*/
#endif
}

WrExternalImage LockExternalImage(void* aObj, WrExternalImageId aId, uint8_t aChannelIndex)
{
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);

  if (texture->AsBufferTextureHost()) {
    RenderBufferTextureHost* bufferTexture = texture->AsBufferTextureHost();
    MOZ_ASSERT(bufferTexture);
    bufferTexture->Lock();
    RenderBufferTextureHost::RenderBufferData data =
        bufferTexture->GetBufferDataForRender(aChannelIndex);

    return RawDataToWrExternalImage(data.mData, data.mBufferSize);
  } else {
    printf_stderr("bignose LockExternalImage, gl\n");
    // texture handle case
    RenderTextureHostOGL* textureOGL = texture->AsTextureHostOGL();
    MOZ_ASSERT(textureOGL);

    textureOGL->SetGLContext(renderer->mGL);
    textureOGL->Lock();
    gfx::IntSize size = textureOGL->GetSize(aChannelIndex);

    return NativeTextureToWrExternalImage(textureOGL->GetGLHandle(aChannelIndex),
                                          0, 0,
                                          size.width, size.height);
  }
}

void UnlockExternalImage(void* aObj, WrExternalImageId aId, uint8_t aChannelIndex)
{
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);
  MOZ_ASSERT(texture);
  texture->Unlock();
}

RendererOGL::RendererOGL(RefPtr<RenderThread>&& aThread,
                         RefPtr<gl::GLContext>&& aGL,
                         RefPtr<widget::CompositorWidget>&& aWidget,
                         wr::WindowId aWindowId,
                         WrRenderer* aWrRenderer,
                         layers::CompositorBridgeParentBase* aBridge)
  : mThread(aThread)
  , mGL(aGL)
  , mWidget(aWidget)
  , mWrRenderer(aWrRenderer)
  , mBridge(aBridge)
  , mWindowId(aWindowId)
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mGL);
  MOZ_ASSERT(mWidget);
  MOZ_ASSERT(mWrRenderer);
  MOZ_ASSERT(mBridge);
  MOZ_COUNT_CTOR(RendererOGL);

#ifdef XP_WIN
  if (aGL->IsANGLE()) {
    gl::GLLibraryEGL* egl = &gl::sEGLLibrary;

    // Fetch the D3D11 device.
    EGLDeviceEXT eglDevice = nullptr;
    egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
    MOZ_ASSERT(eglDevice);
    ID3D11Device* device = nullptr;
    egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE, (EGLAttrib*)&device);
    MOZ_ASSERT(device);

    mSyncObject = layers::RendererSyncObject::CreateRendererSyncObject(device);
  }
#endif
}

RendererOGL::~RendererOGL()
{
  MOZ_COUNT_DTOR(RendererOGL);
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current during destroying.";
    // Leak resources!
    return;
  }
  wr_renderer_delete(mWrRenderer);
}

WrExternalImageHandler
RendererOGL::GetExternalImageHandler()
{
  return WrExternalImageHandler {
    this,
    LockExternalImage,
    UnlockExternalImage,
  };
}

void
RendererOGL::Update()
{
  wr_renderer_update(mWrRenderer);
}

bool
RendererOGL::Render()
{
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    // XXX This could cause oom in webrender since pending_texture_updates is not handled.
    // It needs to be addressed.
    return false;
  }

  mozilla::widget::WidgetRenderingContext widgetContext;

#if defined(XP_MACOSX)
  widgetContext.mGL = mGL;
// TODO: we don't have a notion of compositor here.
//#elif defined(MOZ_WIDGET_ANDROID)
//  widgetContext.mCompositor = mCompositor;
#endif

  if (!mWidget->PreRender(&widgetContext)) {
    // XXX This could cause oom in webrender since pending_texture_updates is not handled.
    // It needs to be addressed.
    return false;
  }
  // XXX set clear color if MOZ_WIDGET_ANDROID is defined.

  auto size = mWidget->GetClientSize();

  if (mSyncObject) {
    // XXX: if the synchronization is failed, we should handle the device reset.
    mSyncObject->Synchronize();
  }

  wr_renderer_render(mWrRenderer, size.width, size.height);

  //TestAngle(mGL.get());

  mGL->SwapBuffers();
  mWidget->PostRender(&widgetContext);

  // TODO: Flush pending actions such as texture deletions/unlocks and
  //       textureHosts recycling.

  return true;
}

void
RendererOGL::Pause()
{
#ifdef MOZ_WIDGET_ANDROID
  if (!mGL || mGL->IsDestroyed()) {
    return;
  }
  // ReleaseSurface internally calls MakeCurrent.
  mGL->ReleaseSurface();
#endif
}

bool
RendererOGL::Resume()
{
#ifdef MOZ_WIDGET_ANDROID
  if (!mGL || mGL->IsDestroyed()) {
    return false;
  }
  // RenewSurface internally calls MakeCurrent.
  return mGL->RenewSurface(mWidget);
#else
  return true;
#endif
}

void
RendererOGL::SetProfilerEnabled(bool aEnabled)
{
  wr_renderer_set_profiler_enabled(mWrRenderer, aEnabled);
}

WrRenderedEpochs*
RendererOGL::FlushRenderedEpochs()
{
  return wr_renderer_flush_rendered_epochs(mWrRenderer);
}

RenderTextureHost*
RendererOGL::GetRenderTexture(WrExternalImageId aExternalImageId)
{
  return mThread->GetRenderTexture(aExternalImageId);
}

} // namespace wr
} // namespace mozilla
