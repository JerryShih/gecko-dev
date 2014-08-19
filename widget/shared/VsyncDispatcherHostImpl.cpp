/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherHostImpl.h"
#include "mozilla/layers/VsyncEventParent.h"
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

VsyncDispatcherHost*
VsyncDispatcherHostImpl::AsVsyncDispatcherHost()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  return this;
}

void
VsyncDispatcherHostImpl::Startup()
{
  MOZ_ASSERT(NS_IsMainThread(), "VDHost StartUp() should in main thread.");
  MOZ_ASSERT(!mInited, "VDHost is already initialized.");

  mInited = true;

  // Init all vsync event registry;
  mVsyncEventParent = new VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>(this);
  mInputDispatcher = new VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>(this);
  mRefreshDriver = new VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>(this);
  mCompositor = new VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>(this);

  // Get platform dependent vsync timer.
  // We only use the vsync timer at Chrome process.
  // Content side doesn't need to do this. Chrome will send the vsync event
  // to content via PVsyncEvent ipc protocol.
  mTimer = PlatformVsyncTimerFactory::Create(this);
  MOZ_ASSERT(mTimer);
  mVsyncRate = mTimer->GetVsyncRate();
  MOZ_ASSERT(mVsyncRate);
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
  delete mRefreshDriver;
  mRefreshDriver = nullptr;
  delete mInputDispatcher;
  mInputDispatcher = nullptr;
  delete mVsyncEventParent;
  mVsyncEventParent = nullptr;

  mInited = false;

  // Release the VsyncDispatcher singleton.
  sVsyncDispatcherHost = nullptr;
}

VsyncDispatcherHostImpl::VsyncDispatcherHostImpl()
  : mVsyncRate(0)
  , mInited(false)
  , mVsyncEventNeeded(false)
  , mVsyncEventParent(nullptr)
  , mInputDispatcher(nullptr)
  , mRefreshDriver(nullptr)
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
VsyncDispatcherHostImpl::GetVsyncEventParentRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mInputDispatcher);

  return mInputDispatcher;
}

VsyncEventRegistry*
VsyncDispatcherHostImpl::GetInputDispatcherRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mInputDispatcher);

  return mInputDispatcher;
}

VsyncEventRegistry*
VsyncDispatcherHostImpl::GetRefreshDriverRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mRefreshDriver);

  return mRefreshDriver;
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
  MOZ_ASSERT(IsInVsyncDispatcherThread());
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

  // Send vsync event to content process
  mVsyncEventParent->Dispatch(mCurrentTimestamp, mCurrentFrameNumber);

  // Dispatch vsync event to chrome refresh driver.
  mRefreshDriver->Dispatch(mCurrentTimestamp, mCurrentFrameNumber);
}

void
VsyncDispatcherHostImpl::VsyncTickNeeded()
{
//  MOZ_ASSERT(mTimer);
//  MOZ_ASSERT(IsInVsyncDispatcherThread());
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

bool
VsyncDispatcherHostImpl::IsInVsyncDispatcherThread() const
{
  //return (mVsyncDispatchHostThread->thread_id() == PlatformThread::CurrentId());

  return true;
}

uint32_t
VsyncDispatcherHostImpl::GetVsyncRate() const
{
  MOZ_ASSERT(mVsyncRate);

  return mVsyncRate;
}

} // namespace mozilla
