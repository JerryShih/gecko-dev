/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherClient.h"
#include "VsyncDispatcherHelper.h"
#include "mozilla/layers/VsyncEventChild.h"
#include "base/message_loop.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla {

using namespace layers;

scoped_refptr<VsyncDispatcherClient> VsyncDispatcherClient::mVsyncDispatcherClient;

/*static*/ VsyncDispatcherClient*
VsyncDispatcherClient::GetInstance()
{
  if (!mVsyncDispatcherClient) {
    // Because we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION,
    // the first call of GetInstance() should at main thread.
    mVsyncDispatcherClient = new VsyncDispatcherClient();
    MOZ_RELEASE_ASSERT(mVsyncDispatcherClient, "Create VDClient failed.");
  }

  return mVsyncDispatcherClient;
}

void
VsyncDispatcherClient::Startup()
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread(), "Call VDClient at wrong thread.");
  MOZ_ASSERT(!mInited, "VDClient is already initialized.");

  mInited = true;
}

void
VsyncDispatcherClient::Shutdown()
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread());
  MOZ_ASSERT(mInited, "VDClient is not initialized.");

  mInited = false;

  // Release the VsyncDispatcherClient singleton.
  mVsyncDispatcherClient = nullptr;
}

VsyncDispatcherClient::VsyncDispatcherClient()
  : mVsyncRate(0)
  , mInited(false)
  , mVsyncEventChild(nullptr)
  , mVsyncEventNeeded(false)
{
}

VsyncDispatcherClient::~VsyncDispatcherClient()
{
  mVsyncEventChild = nullptr;
}

RefreshDriverTrigger*
VsyncDispatcherClient::AsRefreshDriverTrigger()
{
  return this;
}

void
VsyncDispatcherClient::DispatchVsyncEvent(int64_t aTimestampUS, uint32_t aFrameNumber)
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
VsyncDispatcherClient::TickRefreshDriver(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  //TODO: tick all registered refresh driver in content process
}

int
VsyncDispatcherClient::GetVsyncObserverCount() const
{
  //TODO: If we have more type of vsync observer, we need to count all of them here

  return 0;
}

void
VsyncDispatcherClient::SetVsyncEventChild(VsyncEventChild* aVsyncEventChild)
{
  MOZ_ASSERT(mInited, "VDClient is not initialized.");
  MOZ_ASSERT(IsInVsyncDispatcherClientThread(), "Call SetVsyncEventChild at wrong thread.");

  mVsyncEventChild = aVsyncEventChild;
}

void
VsyncDispatcherClient::EnableVsyncEvent(bool aEnable)
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
VsyncDispatcherClient::EnableVsyncNotificationIfhasObserver()
{
  // We check the observer number here to enable/disable vsync ipc notificatoin
  if (!!GetVsyncObserverCount() !=  mVsyncEventNeeded) {
    mVsyncEventNeeded = !mVsyncEventNeeded;
    EnableVsyncEvent(mVsyncEventNeeded);
  }
}

bool
VsyncDispatcherClient::IsInVsyncDispatcherClientThread()
{
  return NS_IsMainThread();
}

void
VsyncDispatcherClient::RegisterTimer(VsyncObserver* aTimer)
{
  //TODO: register a refresh driver timer
}

void
VsyncDispatcherClient::UnregisterTimer(VsyncObserver* aTimer, bool aSync)
{
  //TODO: unregister a refresh driver timer.
  //      If we use sync mode, We should be blocked here until
  //      we actually remove the timer at vsync dispatcher thread.
}

void
VsyncDispatcherClient::SetVsyncRate(uint32_t aVsyncRate)
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread());

  mVsyncRate = aVsyncRate;
}

uint32_t
VsyncDispatcherClient::GetVsyncRate()
{
  MOZ_ASSERT(IsInVsyncDispatcherClientThread());

  return mVsyncRate;
}

} // namespace mozilla
