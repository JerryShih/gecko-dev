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
#include "VsyncDispatcherTrace.h"

// Debug
#include "cutils/properties.h"
#include "mozilla/VsyncDispatcherTrace.h"

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

  virtual uint32_t GetObserverNum() const MOZ_OVERRIDE;

  // Tick all registered refresh driver
  void Dispatch(int64_t aTimestampNanosecond,
                TimeStamp aTimestamp,
                int64_t aTimestampJS,
                uint64_t aFrameNumber);

private:
  void EnableVsyncNotificationIfhasObserver();

  bool IsInVsyncDispatcherThread();

private:
  VsyncDispatcherClientImpl* mVsyncDispatcher;

  typedef nsTArray<VsyncObserver*> ObserverList;
  ObserverList mObserverListList;
};

RefreshDriverRegistryClient::RefreshDriverRegistryClient(VsyncDispatcherClientImpl* aVsyncDispatcher)
  : mVsyncDispatcher(aVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mVsyncDispatcher);
}

RefreshDriverRegistryClient::~RefreshDriverRegistryClient()
{
  MOZ_ASSERT(NS_IsMainThread());
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

uint32_t
RefreshDriverRegistryClient::GetObserverNum() const
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  return mObserverListList.Length();
}

void
RefreshDriverRegistryClient::Dispatch(int64_t aTimestampNanosecond,
                                      TimeStamp aTimestamp,
                                      int64_t aTimestampJS,
                                      uint64_t aFrameNumber)
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  // We might change the mObserverListList in TickVsync(). In content process,
  // we do Dispatch() and TickVsync() at the same thread, so we keep a copy here.
  ObserverList tempRefreshDriverTimerList(mObserverListList);

  // Our TickVsync() is one-shot. We will not tick the refresh driver unless it
  // registers again.
  mObserverListList.Clear();

  // Tick all content registered refresh drivers.
  // VDClient and refresh driver use the same thread, so we tick the
  // refresh driver directly.
  for (ObserverList::size_type i = 0; i < tempRefreshDriverTimerList.Length(); i++) {
    tempRefreshDriverTimerList[i]->TickVsync(aTimestampNanosecond, aTimestamp, aTimestampJS, aFrameNumber);
  }
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
  , mCurrentTimestampNanosecond(0)
  , mCurrentTimestampJS(0)
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
VsyncDispatcherClientImpl::DispatchVsyncEvent(int64_t aTimestampNanosecond,
                                              TimeStamp aTimestamp,
                                              int64_t aTimestampJS,
                                              uint64_t aFrameNumber)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread(), "Call VDClient::DispatchVsyncEvent at wrong thread.");

  MOZ_ASSERT(aTimestampNanosecond > mCurrentTimestampNanosecond);
  MOZ_ASSERT(aTimestamp > mCurrentTimestamp);
  MOZ_ASSERT(aTimestampJS > mCurrentTimestampJS);
  MOZ_ASSERT(aFrameNumber > mCurrentFrameNumber);

  // End
  char propValue[PROPERTY_VALUE_MAX];
  property_get("silk.ipc", propValue, "0");
  if (atoi(propValue) != 0) {
    property_get("silk.timer.log", propValue, "0");
    if (atoi(propValue) == 0) {
      VSYNC_ASYNC_SYSTRACE_LABEL_END_PRINTF((int32_t)aFrameNumber,
                                            "VsyncToRefreshDriver_IPC (%u)",
                                            (uint32_t)aFrameNumber);
    } else {
      static VsyncLatencyLogger* logger = VsyncLatencyLogger::CreateLogger("Silk Client::DispatchVsyncEvent");
      TimeDuration diff = TimeStamp::Now() - aTimestamp;
      logger->Update(aFrameNumber, (int64_t)diff.ToMicroseconds());
      logger->FlushStat(aFrameNumber);
    }
  }

  mCurrentTimestampNanosecond = aTimestampNanosecond;
  mCurrentTimestamp = aTimestamp;
  mCurrentTimestampJS = aTimestampJS;
  mCurrentFrameNumber = aFrameNumber;

  // Scope
  property_get("silk.r.scope.client", propValue, "0");
  if (atoi(propValue) != 0) {
    property_get("silk.timer.log", propValue, "0");
    static VsyncLatencyLogger* logger = nullptr;
    if (atoi(propValue) != 0) {
      logger = VsyncLatencyLogger::CreateLogger("Silk Client RD::TickTask Runtime");
      logger->Start(aFrameNumber);
    } else {
      VSYNC_SCOPED_SYSTRACE_LABEL_PRINTF("RefreshDriver.client (%u)", (uint32_t)aFrameNumber);
    }

    if (!mVsyncEventNeeded) {
      // If we received vsync event but there is no observer here, we disable
      // the vsync event again.
      EnableVsyncEvent(false);
      return;
    }

    TickRefreshDriver();

    if (atoi(propValue) != 0 && logger) {
      logger->End(aFrameNumber);
      logger->FlushStat(aFrameNumber);
    }
  } else {
    if (!mVsyncEventNeeded) {
      // If we received vsync event but there is no observer here, we disable
      // the vsync event again.
      EnableVsyncEvent(false);
      return;
    }

    TickRefreshDriver();
  }

  mVsyncEventNeeded = (GetVsyncObserverCount() > 0);
}

void
VsyncDispatcherClientImpl::TickRefreshDriver()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());
  MOZ_ASSERT(mRefreshDriver);

  mRefreshDriver->Dispatch(mCurrentTimestampNanosecond, mCurrentTimestamp, mCurrentTimestampJS, mCurrentFrameNumber);
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
