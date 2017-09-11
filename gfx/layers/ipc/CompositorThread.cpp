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
#include "mozilla/StackWalk.h"

extern "C" {
static void
PrintCompositorThreadHolderStackFrame(uint32_t aFrameNumber, void* aPC, void* aSP, void* aClosure)
{
  MozCodeAddressDetails details;

  MozDescribeCodeAddress(aPC, &details);
  if (aFrameNumber == 1) {
    gfxCriticalNote << "gpu:" << XRE_IsGPUProcess() << "," <<
        "parent:" << XRE_IsGPUProcess() << "," <<
        "pid:" << base::GetCurrentProcId() << "," <<
        "tid:" << PlatformThread::CurrentId() << "," <<
        "f:0, addr:" << details.loffset;
  } else {
    gfxCriticalNote << "f:" << aFrameNumber << ",addr:" << details.loffset;
  }
}
}

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
  MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(CompositorThreadHolder);
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count = ++mRefCnt;

  gfxCriticalNote << "[gfx] CompositorThreadHolder::AddRef, " <<
      "gpu:" << XRE_IsGPUProcess() << ", " <<
      "parent:" << XRE_IsGPUProcess() << ", " <<
      "pid:" << base::GetCurrentProcId() << ", " <<
      "tid:" << PlatformThread::CurrentId() << ", " <<
      "ref-count:" << count;
  MozStackWalk(PrintCompositorThreadHolderStackFrame, 2, 5, nullptr);

  return (nsrefcnt) count;
}

void
CompositorThreadHolder::DeleteToBeCalledOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  gfxCriticalNote << "[gfx] CompositorThreadHolder::DeleteToBeCalledOnMainThread, " <<
      "gpu:" << XRE_IsGPUProcess() << ", " <<
      "parent:" << XRE_IsGPUProcess() << ", " <<
      "pid:" << base::GetCurrentProcId() << ", " <<
      "tid:" << PlatformThread::CurrentId();
  MozStackWalk(PrintCompositorThreadHolderStackFrame, 2, 5, nullptr);

  delete this;
}

NS_METHOD_(MozExternalRefCountType)
CompositorThreadHolder::CompositorThreadHolder::Release(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;

  if (count == 0) {
    if (NS_IsMainThread()) {
      DeleteToBeCalledOnMainThread();
      gfxCriticalNote << "[gfx] CompositorThreadHolder::Release, delete directly, " <<
          "gpu:" << XRE_IsGPUProcess() << ", " <<
          "parent:" << XRE_IsGPUProcess() << ", " <<
          "pid:" << base::GetCurrentProcId() << ", " <<
          "tid:" << PlatformThread::CurrentId();
      MozStackWalk(PrintCompositorThreadHolderStackFrame, 2, 5, nullptr);
    } else {
      NS_DispatchToMainThread(
          new mozilla::layers::DeleteOnMainThreadTask<CompositorThreadHolder>(this));
      gfxCriticalNote << "[gfx] CompositorThreadHolder::Release, defer, " <<
          "gpu:" << XRE_IsGPUProcess() << ", " <<
          "parent:" << XRE_IsGPUProcess() << ", " <<
          "pid:" << base::GetCurrentProcId() << ", " <<
          "tid:" << PlatformThread::CurrentId();
      MozStackWalk(PrintCompositorThreadHolderStackFrame, 2, 5, nullptr);
    }
  } else {
    gfxCriticalNote << "[gfx] CompositorThreadHolder::Release, " <<
        "gpu:" << XRE_IsGPUProcess() << ", " <<
        "parent:" << XRE_IsGPUProcess() << ", " <<
        "pid:" << base::GetCurrentProcId() << ", " <<
        "tid:" << PlatformThread::CurrentId() << ", " <<
        "ref-count:" << count;
   MozStackWalk(PrintCompositorThreadHolderStackFrame, 2, 5, nullptr);
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

  gfxCriticalNote << "[gfx] CompositorThreadHolder::Shutdown start, " <<
      "gpu:" << XRE_IsGPUProcess() << ", " <<
      "parent:" << XRE_IsGPUProcess() << ", " <<
      "pid:" << base::GetCurrentProcId() << ", " <<
      "tid:" << PlatformThread::CurrentId();

  // No locking is needed around sFinishedCompositorShutDown because it is only
  // ever accessed on the main thread.
  SpinEventLoopUntil([&]() { return sFinishedCompositorShutDown; });

  gfxCriticalNote << "[gfx] CompositorThreadHolder::Shutdown end," <<
      "gpu:" << XRE_IsGPUProcess() << ", " <<
      "parent:" << XRE_IsGPUProcess() << ", " <<
      "pid:" << base::GetCurrentProcId() << ", " <<
      "tid:" << PlatformThread::CurrentId();

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
