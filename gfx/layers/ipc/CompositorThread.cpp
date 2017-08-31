/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorThread.h"
#include "MainThreadUtils.h"
#include "nsThreadUtils.h"
#include "CompositorBridgeParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/media/MediaSystemResourceService.h"

namespace mozilla {

namespace gfx {
// See VRManagerChild.cpp
void ReleaseVRManagerParentSingleton();
} // namespace gfx

namespace layers {

static StaticRefPtr<CompositorThreadHolder> sCompositorThreadHolder;
static bool sFinishedCompositorShutDown = false;

// See ImageBridgeChild.cpp
void ReleaseImageBridgeParentSingleton();


NS_METHOD_(MozExternalRefCountType)
CompositorThreadHolder::AddRef(void)
{
  MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(CompositorThreadHolder)
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count = ++mRefCnt;

  printf_stderr("@bignose CompositorThreadHolder::AddRef, count:%d\n",count);
  NS_ASSERTION2(false, "@bignose CompositorThreadHolder::AddRef");

  return (nsrefcnt) count;
}

void
CompositorThreadHolder::DeleteToBeCalledOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ASSERTION2(false, "@bignose CompositorThreadHolder::DeleteToBeCalledOnMainThread");

  delete this;
}

NS_METHOD_(MozExternalRefCountType)
CompositorThreadHolder::CompositorThreadHolder::Release(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;

  printf_stderr("@bignose gpu:%d parent:%d pid::%d tid:%d CompositorThreadHolder::Release, count:%d\n",
      (int)XRE_IsGPUProcess(), (int)XRE_IsParentProcess(), base::GetCurrentProcId(), PlatformThread::CurrentId(), count);

  if (count == 0) {
    if (NS_IsMainThread()) {
      printf_stderr("@bignose gpu:%d parent:%d pid::%d tid:%d CompositorThreadHolder::Release delete immediately\n",
          (int)XRE_IsGPUProcess(), (int)XRE_IsParentProcess(), base::GetCurrentProcId(), PlatformThread::CurrentId());
      NS_ASSERTION2(false, "@bignose CompositorThreadHolder::Release delete immediately");
      DeleteToBeCalledOnMainThread();
    } else {
      printf_stderr("@bignose gpu:%d parent:%d pid::%d tid:%d CompositorThreadHolder::Release defer delete\n",
          (int)XRE_IsGPUProcess(), (int)XRE_IsParentProcess(), base::GetCurrentProcId(), PlatformThread::CurrentId());
      NS_ASSERTION2(false, "@bignose CompositorThreadHolder::Release defer delete");
      NS_DispatchToMainThread(
          new mozilla::layers::DeleteOnMainThreadTask<CompositorThreadHolder>(this));
    }
  } else {
    NS_ASSERTION2(false, "@bignose CompositorThreadHolder::Release");
  }
  return count;
}

CompositorThreadHolder* GetCompositorThreadHolder()
{
  return sCompositorThreadHolder;
}

base::Thread*
CompositorThread()
{
  return sCompositorThreadHolder
         ? sCompositorThreadHolder->GetCompositorThread()
         : nullptr;
}

/* static */ MessageLoop*
CompositorThreadHolder::Loop()
{
  return CompositorThread() ? CompositorThread()->message_loop() : nullptr;
}

CompositorThreadHolder*
CompositorThreadHolder::GetSingleton()
{
  return sCompositorThreadHolder;
}

CompositorThreadHolder::CompositorThreadHolder()
  : mCompositorThread(CreateCompositorThread())
{
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorThreadHolder::~CompositorThreadHolder()
{
  MOZ_ASSERT(NS_IsMainThread());

  DestroyCompositorThread(mCompositorThread);
}

/* static */ void
CompositorThreadHolder::DestroyCompositorThread(base::Thread* aCompositorThread)
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder, "We shouldn't be destroying the compositor thread yet.");

  CompositorBridgeParent::Shutdown();
  delete aCompositorThread;
  sFinishedCompositorShutDown = true;
}

/* static */ base::Thread*
CompositorThreadHolder::CreateCompositorThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder, "The compositor thread has already been started!");

  base::Thread* compositorThread = new base::Thread("Compositor");

  base::Thread::Options options;
  /* Timeout values are powers-of-two to enable us get better data.
     128ms is chosen for transient hangs because 8Hz should be the minimally
     acceptable goal for Compositor responsiveness (normal goal is 60Hz). */
  options.transient_hang_timeout = 128; // milliseconds
  /* 2048ms is chosen for permanent hangs because it's longer than most
   * Compositor hangs seen in the wild, but is short enough to not miss getting
   * native hang stacks. */
  options.permanent_hang_timeout = 2048; // milliseconds
#if defined(_WIN32)
  /* With d3d9 the compositor thread creates native ui, see DeviceManagerD3D9. As
   * such the thread is a gui thread, and must process a windows message queue or
   * risk deadlocks. Chromium message loop TYPE_UI does exactly what we need. */
  options.message_loop_type = MessageLoop::TYPE_UI;
#endif

  if (!compositorThread->StartWithOptions(options)) {
    delete compositorThread;
    return nullptr;
  }

  CompositorBridgeParent::Setup();
  ImageBridgeParent::Setup();

  return compositorThread;
}

void
CompositorThreadHolder::Start()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  MOZ_ASSERT(!sCompositorThreadHolder, "The compositor thread has already been started!");

  sCompositorThreadHolder = new CompositorThreadHolder();
}

void
CompositorThreadHolder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  MOZ_ASSERT(sCompositorThreadHolder, "The compositor thread has already been shut down!");

  ReleaseImageBridgeParentSingleton();
  gfx::ReleaseVRManagerParentSingleton();
  MediaSystemResourceService::Shutdown();

  sCompositorThreadHolder = nullptr;
  printf_stderr("@bignose gpu:%d parent:%d pid::%d tid:%d CompositorThreadHolder::Shutdown for sCompositorThreadHolder start\n",
      (int)XRE_IsGPUProcess(), (int)XRE_IsParentProcess(), base::GetCurrentProcId(), PlatformThread::CurrentId());

  // No locking is needed around sFinishedCompositorShutDown because it is only
  // ever accessed on the main thread.
  SpinEventLoopUntil([&]() {
    printf_stderr("@bignose gpu:%d parent:%d pid::%d tid:%d CompositorThreadHolder::Shutdown for sCompositorThreadHolder in spin loop\n",
        (int)XRE_IsGPUProcess(), (int)XRE_IsParentProcess(), base::GetCurrentProcId(), PlatformThread::CurrentId());

    return sFinishedCompositorShutDown;
    //return false;
  });

  printf_stderr("@bignose gpu:%d parent:%d pid::%d tid:%d CompositorThreadHolder::Shutdown for sCompositorThreadHolder end\n",
      (int)XRE_IsGPUProcess(), (int)XRE_IsParentProcess(), base::GetCurrentProcId(), PlatformThread::CurrentId());

  CompositorBridgeParent::FinishShutdown();
}

/* static */ bool
CompositorThreadHolder::IsInCompositorThread()
{
  return CompositorThread() &&
         CompositorThread()->thread_id() == PlatformThread::CurrentId();
}

} // namespace mozilla
} // namespace layers

bool
NS_IsInCompositorThread()
{
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}
