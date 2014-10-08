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
{

}

VsyncDispatcher::~VsyncDispatcher()
{
  MutexAutoLock lock(mCompositorObserverLock);
  mCompositorObservers.Clear();
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

  if (gfxPrefs::VsyncAlignedCompositor()) {
    MutexAutoLock lock(mCompositorObserverLock);
    NotifyVsync(aVsyncTimestamp, mCompositorObservers);
  }

  // TODO: notify nsRefreshDriver
}

void
VsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp, nsTArray<VsyncObserver*>& aObservers)
{
  // Callers should lock the respective lock for the aObservers before calling this function
  for (size_t i = 0; i < aObservers.Length(); i++) {
    aObservers[i]->NotifyVsync(aVsyncTimestamp);
  }
}

void
VsyncDispatcher::AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mCompositorObserverLock);
  mCompositorObservers.AppendElement(aVsyncObserver);
}

void
VsyncDispatcher::RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mCompositorObserverLock);
  if (mCompositorObservers.Contains(aVsyncObserver)) {
    mCompositorObservers.RemoveElement(aVsyncObserver);
  }
  //mCompositorObservers.RemoveElement(aVsyncObserver);
  /*
  for (size_t i = 0; i < mCompositorObservers.Length(); i++) {
    VsyncObserver* observer = mCompositorObservers[i];
    if (observer == aVsyncObserver) {
      mCompositorObservers.RemoveElement();
      return;
    }
  }
  */

  NS_WARNING("Could not delete compositor observer\n");
}

} // namespace mozilla
