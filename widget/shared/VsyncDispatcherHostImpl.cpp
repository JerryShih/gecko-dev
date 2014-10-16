/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherHostImpl.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "VsyncDispatcherHelper.h"

namespace mozilla {

using namespace layers;

StaticRefPtr<VsyncDispatcherHostImpl> VsyncDispatcherHostImpl::sVsyncDispatcherHost;

/*static*/ VsyncDispatcherHostImpl*
VsyncDispatcherHostImpl::GetInstance()
{
  if (!sVsyncDispatcherHost) {
    // Because we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION,
    // the first call of GetInstance() should at main thread.
    sVsyncDispatcherHost = new VsyncDispatcherHostImpl();
  }

  return sVsyncDispatcherHost;
}

void
VsyncDispatcherHostImpl::Startup()
{
  MOZ_ASSERT(NS_IsMainThread(), "VDHost StartUp() should in main thread.");
  MOZ_ASSERT(!mInited, "VDHost is already initialized.");

  mInited = true;

  // Init all vsync event registry;
  mInputDispatcher = new VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>(this);
  mCompositor = new VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>(this);

  // Get platform dependent vsync timer.
  // We only use the vsync timer at Chrome process.
  // Content side doesn't need to do this. Chrome will send the vsync event
  // to content via PVsyncEvent ipc protocol.
  mTimer = PlatformVsyncTimerFactory::Create(this);
  MOZ_ASSERT(mTimer);
  // Start the timer.
  mTimer->Enable(true);
}

void
VsyncDispatcherHostImpl::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "VDHost ShutDown() should in main thread.");
  MOZ_ASSERT(mInited, "VDHost is not initialized.");

  mTimer->Shutdown();
  delete mTimer;
  mTimer = nullptr;

  delete mCompositor;
  mCompositor = nullptr;
  delete mInputDispatcher;
  mInputDispatcher = nullptr;

  mInited = false;

  // Release the VsyncDispatcher singleton.
  sVsyncDispatcherHost = nullptr;
}

VsyncDispatcherHostImpl::VsyncDispatcherHostImpl()
  : mInited(false)
  , mVsyncEventNeeded(false)
  , mInputDispatcher(nullptr)
  , mCompositor(nullptr)
  , mTimer(nullptr)
  , mCurrentFrameNumber(0)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDispatcherHostImpl::~VsyncDispatcherHostImpl()
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncEventRegistry*
VsyncDispatcherHostImpl::GetInputDispatcherRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mInputDispatcher);

  return mInputDispatcher;
}

VsyncEventRegistry*
VsyncDispatcherHostImpl::GetCompositorRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mCompositor);

  return mCompositor;
}

void
VsyncDispatcherHostImpl::NotifyVsync(TimeStamp aTimestamp)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(aTimestamp > mCurrentTimestamp);

  mCurrentTimestamp = aTimestamp;
  ++mCurrentFrameNumber;

  if (!mVsyncEventNeeded) {
    return;
  }

  // Dispatch vsync event to input dispatcher
  mInputDispatcher->Dispatch(mCurrentTimestamp, mCurrentFrameNumber);

  // Dispatch vsync event to compositor.
  mCompositor->Dispatch(mCurrentTimestamp, mCurrentFrameNumber);
}

void
VsyncDispatcherHostImpl::VsyncTickNeeded()
{
//  MOZ_ASSERT(mTimer);
//
//  // We check the observer number here to enable/disable vsync event
//  if (!!GetVsyncObserverCount() !=  mVsyncEventNeeded) {
//    mVsyncEventNeeded = !mVsyncEventNeeded;
//
//    mTimer->Enable(mVsyncEventNeeded);
//  }

  mVsyncEventNeeded = true;
  mTimer->Enable(mVsyncEventNeeded);
}

} // namespace mozilla
