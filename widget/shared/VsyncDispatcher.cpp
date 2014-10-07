/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "nsXULAppAPI.h"
#include "base/thread.h"
#include "nsThreadUtils.h" 
#include "gfxPrefs.h"
#include "mozilla/ClearOnShutdown.h"
#include "GeckoTouchDispatcher.h"

namespace mozilla {

StaticRefPtr<VsyncDispatcher> VsyncDispatcher::sVsyncDispatcher;

/*static*/ VsyncDispatcher*
VsyncDispatcher::GetInstance()
{
  if (!sVsyncDispatcher) {
    sVsyncDispatcher= new VsyncDispatcher();
    ClearOnShutdown(&sVsyncDispatcher);
  }

  return sVsyncDispatcher;
}

VsyncDispatcher::VsyncDispatcher()
  : mCompositorObserverLock("CompositorObserverLock")
  , mCompositorParent(nullptr)
{

}

uint32_t
VsyncDispatcher::GetVsyncRate()
{
  // TODO hook into hwc
  return 60;
}

void
VsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp, nsecs_t aAndroidVsyncTime)
{
  if (gfxPrefs::TouchResampling()) {
    GeckoTouchDispatcher::NotifyVsync(aAndroidVsyncTime);
  }
  if (gfxPrefs::VsyncAlignedCompositor() && mCompositorParent) {
    MutexAutoLock lock(mCompositorObserverLock);
    mCompositorParent->NotifyVsync(aVsyncTimestamp);
  }
}

void
VsyncDispatcher::AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mCompositorObserverLock);
  mCompositorParent = aVsyncObserver;  
}

void
VsyncDispatcher::RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mCompositorObserverLock);
  mCompositorParent = nullptr;
}

} // namespace mozilla
