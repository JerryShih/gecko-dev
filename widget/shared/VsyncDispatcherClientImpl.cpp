/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
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

// The vsync event registry for RefreshDriver
class RefreshDriverRegistryClient MOZ_FINAL : public VsyncEventRegistry
{
  friend class ObserverListHelper;

public:
  RefreshDriverRegistryClient(VsyncDispatcherClientImpl* aVsyncDispatcher);
  ~RefreshDriverRegistryClient();

  virtual void Register(VsyncObserver* aVsyncObserver) MOZ_OVERRIDE;
  virtual void Unregister(VsyncObserver* aVsyncObserver, bool aSync) MOZ_OVERRIDE;

  // Tick all registered refresh driver
  void Dispatch(int64_t aTimestampUS, uint64_t aFrameNumber);

private:
  void EnableVsyncNotificationIfhasObserver();

  bool IsInVsyncDispatcherThread();

  VsyncDispatcherClientImpl* mVsyncDispatcher;
};

RefreshDriverRegistryClient::RefreshDriverRegistryClient(VsyncDispatcherClientImpl* aVsyncDispatcher)
  : mVsyncDispatcher(aVsyncDispatcher)
{
  MOZ_ASSERT(mVsyncDispatcher);
}

RefreshDriverRegistryClient::~RefreshDriverRegistryClient()
{
  mObserverListList.Clear();
}

void
RefreshDriverRegistryClient::Register(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  ObserverListHelper::Add(this, &mObserverListList, aVsyncObserver);
}

void
RefreshDriverRegistryClient::Unregister(VsyncObserver* aVsyncObserver, bool aSync)
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  // We only call unregister at vsync dispatcher thread, so ignore the sync flag
  // here.
  ObserverListHelper::Remove(this, &mObserverListList, aVsyncObserver);
}

void
RefreshDriverRegistryClient::Dispatch(int64_t aTimestampUS, uint64_t aFrameNumber)
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  // TODO: tick all refresh driver.
}

void
RefreshDriverRegistryClient::EnableVsyncNotificationIfhasObserver()
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  mVsyncDispatcher->EnableVsyncNotificationIfhasObserver();
}

bool
RefreshDriverRegistryClient::IsInVsyncDispatcherThread()
{
  return mVsyncDispatcher->IsInVsyncDispatcherThread();
}

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
  MOZ_ASSERT(NS_IsMainThread(), "Call VDClient Startup() at wrong thread.");
  MOZ_ASSERT(!mInited, "VDClient is already initialized.");

  mInited = true;

  mRefreshDriver = new RefreshDriverRegistryClient(this);
}

void
VsyncDispatcherClientImpl::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Call VDClient Shutdown() at wrong thread.");
  MOZ_ASSERT(mInited, "VDClient is not initialized.");

  mInited = false;

  // Release the VsyncDispatcherClient singleton.
  sVsyncDispatcherClient = nullptr;

  delete mRefreshDriver;
  mRefreshDriver = nullptr;
}

VsyncDispatcherClientImpl::VsyncDispatcherClientImpl()
  : mVsyncRate(0)
  , mInited(false)
  , mVsyncEventNeeded(false)
  , mVsyncEventChild(nullptr)
  , mRefreshDriver(nullptr)
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
  MOZ_ASSERT(mInited);

  return this;
}

VsyncEventRegistry*
VsyncDispatcherClientImpl::GetRefreshDriverRegistry()
{
  MOZ_ASSERT(mRefreshDriver);

  return mRefreshDriver;
}

void
VsyncDispatcherClientImpl::DispatchVsyncEvent(int64_t aTimestampUS, uint64_t aFrameNumber)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread(), "Call VDClient::DispatchVsyncEvent at wrong thread.");

  MOZ_ASSERT(aTimestampUS > mCurrentTimestampUS);
  MOZ_ASSERT(aFrameNumber > aFrameNumber);
  mCurrentTimestampUS = aTimestampUS;
  mCurrentFrameNumber = aFrameNumber;

  if (!mVsyncEventNeeded) {
    // If we received vsync event but there is no observer here, we disable
    // the vsync event again.
    EnableVsyncEvent(false);
    return;
  }

  TickRefreshDriver();

  mVsyncEventNeeded = (GetVsyncObserverCount() > 0);
}

void
VsyncDispatcherClientImpl::TickRefreshDriver()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mRefreshDriver->Dispatch(mCurrentTimestampUS, mCurrentFrameNumber);
}

int
VsyncDispatcherClientImpl::GetVsyncObserverCount()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  return mRefreshDriver->GetObserverNum();
}

void
VsyncDispatcherClientImpl::SetVsyncEventChild(VsyncEventChild* aVsyncEventChild)
{
  MOZ_ASSERT(mInited, "VDClient is not initialized.");
  MOZ_ASSERT(IsInVsyncDispatcherThread(), "Call SetVsyncEventChild at wrong thread.");

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

void
VsyncDispatcherClientImpl::EnableVsyncNotificationIfhasObserver()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  // We check the observer number here to enable/disable vsync ipc notificatoin
  if (!!GetVsyncObserverCount() !=  mVsyncEventNeeded) {
    mVsyncEventNeeded = !mVsyncEventNeeded;
    EnableVsyncEvent(mVsyncEventNeeded);
  }
}

bool
VsyncDispatcherClientImpl::IsInVsyncDispatcherThread()
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
  MOZ_ASSERT(mVsyncRate);

  return mVsyncRate;
}

} // namespace mozilla
