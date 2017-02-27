/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererOGL.h"
#include "CompositableHost.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/WebRenderCompositableHolder.h"

namespace mozilla {
namespace wr {

WrExternalImage LockExternalImage(void* aRendererOGL, WrExternalImageId aId)
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_ASSERT(aRendererOGL);
  RendererOGL* renderer = static_cast<RendererOGL*>(aRendererOGL);
  layers::CompositableHost* compositable = renderer->mCompositable[aId.id].get();
  MOZ_ASSERT(compositable);
  // XXX: only suppurt buffer texture now.
  // TODO: handle gl texture here.
  layers::TextureHost* textureHost = compositable->GetAsTextureHost();
  layers::BufferTextureHost* bufferTextureHost = textureHost->AsBufferTextureHost();
  MOZ_ASSERT(bufferTextureHost);

  if (bufferTextureHost) {
#if 0
    // dump external image to file
    static int count = 0;
    count;

    std::stringstream sstream;
    sstream << "/tmp/img/" << count << '_' <<
        bufferTextureHost->GetSize().width << '_' << bufferTextureHost->GetSize().height << ".raw";

    FILE* file = fopen(sstream.str().c_str(), "wb");
    if (file) {
      printf_stderr("gecko write image(%d,%d) to file", bufferTextureHost->GetSize().width, bufferTextureHost->GetSize().height);
      fwrite(bufferTextureHost->GetBuffer(), bufferTextureHost->GetBufferSize(), 1, file);
      fclose(file);
    }
#endif

    return WrExternalImage {
      WrExternalImageIdType::MEM_OR_SHMEM,
      0.0f, 0.0f,
      static_cast<float>(bufferTextureHost->GetSize().width), static_cast<float>(bufferTextureHost->GetSize().height),
      0,
      bufferTextureHost->GetBuffer(),
      bufferTextureHost->GetBufferSize()
    };
  }

  return WrExternalImage { WrExternalImageIdType::TEXTURE_HANDLE, 0.0f, 0.0f, 0.0f, 0.0f, 0, nullptr, 0 };
}

void UnlockExternalImage(void* aRendererOGL, WrExternalImageId aId)
{
}

void ReleaseExternalImage(void* aRendererOGL, WrExternalImageId aId)
{
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
}

RendererOGL::~RendererOGL()
{
  MOZ_COUNT_DTOR(RendererOGL);
  mCompositable.clear();
  wr_renderer_delete(mWrRenderer);
}

WrExternalImageHandler
RendererOGL::GetExternalImageHandler()
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  return WrExternalImageHandler {
    this,
    LockExternalImage,
    UnlockExternalImage,
    ReleaseExternalImage,
  };
}

void
RendererOGL::AddExternalImageId(uint64_t aExternalImageId,
                                layers::CompositableHost* aCompositable)
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_ASSERT(mCompositable.count(aExternalImageId) == 0);

  mCompositable[aExternalImageId] = aCompositable;
}

void
RendererOGL::RemoveExternalImageId(uint64_t aExternalImageId)
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_ASSERT(mCompositable.count(aExternalImageId));

  mCompositable.erase(aExternalImageId);
}

void
RendererOGL::Update()
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  wr_renderer_update(mWrRenderer);
}

bool
RendererOGL::Render()
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
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
    return false;
  }
  // XXX set clear color if MOZ_WIDGET_ANDROID is defined.
  // XXX pass the actual render bounds instead of an empty rect.
  mWidget->DrawWindowUnderlay(&widgetContext, LayoutDeviceIntRect());

  auto size = mWidget->GetClientSize();
  wr_renderer_render(mWrRenderer, size.width, size.height);

  mGL->SwapBuffers();
  mWidget->DrawWindowOverlay(&widgetContext, LayoutDeviceIntRect());
  mWidget->PostRender(&widgetContext);

  // TODO: Flush pending actions such as texture deletions/unlocks and
  //       textureHosts recycling.

  return true;
}

void
RendererOGL::SetProfilerEnabled(bool aEnabled)
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  wr_renderer_set_profiler_enabled(mWrRenderer, aEnabled);
}

WrRenderedEpochs*
RendererOGL::FlushRenderedEpochs()
{
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  return wr_renderer_flush_rendered_epochs(mWrRenderer);
}

} // namespace wr
} // namespace mozilla
