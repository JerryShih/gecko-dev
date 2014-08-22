/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherClientImpl.h"
#include "VsyncDispatcherHelper.h"
#include "mozilla/layers/VsyncEventChild.h"
#include "base/message_loop.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla {

using namespace layers;

nsRefPtr<VsyncDispatcherClientImpl> VsyncDispatcherClientImpl::mVsyncDispatcherClient;

/*static*/ VsyncDispatcherClientImpl*
VsyncDispatcherClientImpl::GetInstance()
{
  if (!mVsyncDispatcherClient) {
    // Because we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION,
    // the first call of GetInstance() should at main thread.
    mVsyncDispatcherClient = new VsyncDispatcherClientImpl();
    MOZ_RELEASE_ASSERT(mVsyncDispatcherClient, "Create VDClient failed.");
  }

  return mVsyncDispatcherClient;
}

void
VsyncDispatcherClientImpl::Startup()
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread(), "Call VDClient at wrong thread.");
  MOZ_ASSERT(!mInited, "VDClient is already initialized.");

  mInited = true;
}

void
VsyncDispatcherClientImpl::Shutdown()
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread());
  MOZ_ASSERT(mInited, "VDClient is not initialized.");

  mInited = false;

  // Release the VsyncDispatcherClient singleton.
  mVsyncDispatcherClient = nullptr;
}

VsyncDispatcherClientImpl::VsyncDispatcherClientImpl()
  : mVsyncRate(0)
  , mInited(false)
  , mVsyncEventChild(nullptr)
  , mVsyncEventNeeded(false)
{
}

VsyncDispatcherClientImpl::~VsyncDispatcherClientImpl()
{
  mVsyncEventChild = nullptr;
}

VsyncDispatcherClient*
VsyncDispatcherClientImpl::AsVsyncDispatcherClient()
{
  return this;
}

RefreshDriverTrigger*
VsyncDispatcherClientImpl::AsRefreshDriverTrigger()
{
  return this;
}

void
VsyncDispatcherClientImpl::DispatchVsyncEvent(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  MOZ_ASSERT(mInited, "VDClient is not initialized.");
  MOZ_ASSERT(IsInVsyncDispatcherClientThread(), "Call VDClient::DispatchVsyncEvent at wrong thread.");

  if (!mVsyncEventNeeded) {
    // If we received vsync event but there is no observer here, we disable
    // the vsync event again.
    EnableVsyncEvent(false);
    return;
  }

  TickRefreshDriver(aTimestampUS, aFrameNumber);

  mVsyncEventNeeded = (GetVsyncObserverCount() > 0);
}

void
VsyncDispatcherClientImpl::TickRefreshDriver(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  //TODO: tick all registered refresh driver in content process
}

int
VsyncDispatcherClientImpl::GetVsyncObserverCount() const
{
  //TODO: If we have more type of vsync observer, we need to count all of them here

  return 0;
}

void
VsyncDispatcherClientImpl::SetVsyncEventChild(VsyncEventChild* aVsyncEventChild)
{
  MOZ_ASSERT(mInited, "VDClient is not initialized.");
  MOZ_ASSERT(IsInVsyncDispatcherClientThread(), "Call SetVsyncEventChild at wrong thread.");

  mVsyncEventChild = aVsyncEventChild;
}

void
VsyncDispatcherClientImpl::EnableVsyncEvent(bool aEnable)
{
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
  // We check the observer number here to enable/disable vsync ipc notificatoin
  if (!!GetVsyncObserverCount() !=  mVsyncEventNeeded) {
    mVsyncEventNeeded = !mVsyncEventNeeded;
    EnableVsyncEvent(mVsyncEventNeeded);
  }
}

bool
VsyncDispatcherClientImpl::IsInVsyncDispatcherClientThread()
{
  return NS_IsMainThread();
}

void
VsyncDispatcherClientImpl::RegisterTimer(VsyncObserver* aTimer)
{
  //TODO: register a refresh driver timer
}

void
VsyncDispatcherClientImpl::UnregisterTimer(VsyncObserver* aTimer, bool aSync)
{
  //TODO: unregister a refresh driver timer.
  //      If we use sync mode, We should be blocked here until
  //      we actually remove the timer at vsync dispatcher thread.
}

void
VsyncDispatcherClientImpl::SetVsyncRate(uint32_t aVsyncRate)
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread());

  mVsyncRate = aVsyncRate;
}

uint32_t
VsyncDispatcherClientImpl::GetVsyncRate()
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread());

  return mVsyncRate;
}

} // namespace mozilla
