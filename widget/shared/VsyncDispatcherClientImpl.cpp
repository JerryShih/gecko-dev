/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherClientImpl.h"
#include "mozilla/layers/VsyncEventChild.h"
#include "base/message_loop.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "VsyncDispatcherHelper.h"

namespace mozilla {

using namespace layers;

StaticRefPtr<VsyncDispatcherClientImpl> VsyncDispatcherClientImpl::sVsyncDispatcherClient;

// The vsync event registry for content RefreshDriver.
// RefreshDriverRegistry and VsyncDispatcherClientImpl all run at content
// main thread, so we use VsyncRegistryNonThreadSafePolicy policy.
class RefreshDriverRegistry MOZ_FINAL : public VsyncEventRegistryImpl<VsyncRegistryNonThreadSafePolicy>
{
public:
  RefreshDriverRegistry(VsyncDispatcherClientImpl* aVsyncDispatcher)
    : VsyncEventRegistryImpl<VsyncRegistryNonThreadSafePolicy>(aVsyncDispatcher)
  {
  }

  ~RefreshDriverRegistry() { }

  // Tick all registered refresh driver
  virtual bool Dispatch(TimeStamp aTimestamp, uint64_t aFrameNumber) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

    // In content process, we want to reduce the context switch, so we tick the
    // refresh driver in dispatcher thread directly. We might change the
    // mObserverList or mTemporaryObserverList in VsyncTick(), so we make a
    // copy here.
    ObserverList observerListCopy(mObserverList);
    ObserverList temporaryObserverListCopy(mTemporaryObserverList);

    mTemporaryObserverList.Clear();

    for (ObserverList::size_type i = 0; i < observerListCopy.Length(); ++i) {
      observerListCopy[i]->VsyncTick(aTimestamp, aFrameNumber);
    }

    for (ObserverList::size_type i = 0; i < temporaryObserverListCopy.Length(); ++i) {
      temporaryObserverListCopy[i]->VsyncTick(aTimestamp, aFrameNumber);
    }

    return (GetObserverNum() > 0);
  }
};

/*static*/ VsyncDispatcherClientImpl*
VsyncDispatcherClientImpl::GetInstance()
{
  if (!sVsyncDispatcherClient) {
    // Because we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION,
    // the first call of GetInstance() should at main thread.
    sVsyncDispatcherClient = new VsyncDispatcherClientImpl();
  }

  return sVsyncDispatcherClient;
}

void
VsyncDispatcherClientImpl::Startup()
{
  MOZ_ASSERT(!mInited);
  MOZ_ASSERT(NS_IsMainThread());

  mInited = true;

  mRefreshDriver = new RefreshDriverRegistry(this);
}

void
VsyncDispatcherClientImpl::Shutdown()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(NS_IsMainThread());

  mInited = false;

  delete mRefreshDriver;
  mRefreshDriver = nullptr;

  // Release the VsyncDispatcherClient singleton.
  sVsyncDispatcherClient = nullptr;
}

VsyncDispatcherClientImpl::VsyncDispatcherClientImpl()
  : mVsyncRate(0)
  , mInited(false)
  , mVsyncEventNeeded(false)
  , mVsyncEventChild(nullptr)
  , mRefreshDriver(nullptr)
  , mCurrentFrameNumber(0)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDispatcherClientImpl::~VsyncDispatcherClientImpl()
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDispatcherClient*
VsyncDispatcherClientImpl::AsVsyncDispatcherClient()
{
  MOZ_ASSERT(XRE_GetProcessType() != GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  return this;
}

VsyncEventRegistry*
VsyncDispatcherClientImpl::GetRefreshDriverRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mRefreshDriver);

  return mRefreshDriver;
}

void
VsyncDispatcherClientImpl::NotifyVsync(TimeStamp aTimestamp,
                                       uint64_t aFrameNumber)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  MOZ_ASSERT(aTimestamp > mCurrentTimestamp);
  MOZ_ASSERT(aFrameNumber > mCurrentFrameNumber);

  mCurrentTimestamp = aTimestamp;
  mCurrentFrameNumber = aFrameNumber;

  bool needVsync = false;

  // Dispach vsync event to content refresh driver
  needVsync = needVsync || mRefreshDriver->Dispatch(mCurrentTimestamp,
                                                    mCurrentFrameNumber);

  if (!needVsync) {
    ++mUnusedTickCount;

    if (mUnusedTickCount > MAX_UNUSED_VSYNC_TICK) {
      mUnusedTickCount = 0;
      mVsyncEventNeeded = false;
      EnableVsyncEvent(false);
    }
  } else {
    mUnusedTickCount = 0;
  }
}

void
VsyncDispatcherClientImpl::VsyncTickNeeded()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  if (!mVsyncEventNeeded) {
    mVsyncEventNeeded = true;

    EnableVsyncEvent(true);
  }
}

void
VsyncDispatcherClientImpl::SetVsyncEventChild(VsyncEventChild* aVsyncEventChild)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mVsyncEventChild = aVsyncEventChild;
}

void
VsyncDispatcherClientImpl::EnableVsyncEvent(bool aEnable)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  if (mVsyncEventChild) {
    if (aEnable) {
      mVsyncEventChild->SendRegisterVsyncEvent();
    } else {
      mVsyncEventChild->SendUnregisterVsyncEvent();
    }
  }
}

bool
VsyncDispatcherClientImpl::IsInVsyncDispatcherThread() const
{
  MOZ_ASSERT(mInited);

  return NS_IsMainThread();
}

void
VsyncDispatcherClientImpl::SetVsyncRate(uint32_t aVsyncRate)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mVsyncRate = aVsyncRate;
}

uint32_t
VsyncDispatcherClientImpl::GetVsyncRate() const
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());
  MOZ_ASSERT(mVsyncRate);

  return mVsyncRate;
}

} // namespace mozilla
