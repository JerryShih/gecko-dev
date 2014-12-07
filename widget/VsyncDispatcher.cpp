/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "gfxPrefs.h"
#include "gfxPlatform.h"
#include "mozilla/layers/CompositorParent.h"

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "GeckoProfiler.h"
#include "ProfilerMarkers.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "GeckoTouchDispatcher.h"
#endif

using namespace mozilla::layers;

namespace mozilla {

VsyncDispatcher::VsyncDispatcher()
  : mCompositorObserverLock("CompositorObserverLock")
  , mRefreshDriverObserverLock("RefreshDriverObserverLock")
{
  MOZ_ASSERT(XRE_IsParentProcess());
}

VsyncDispatcher::~VsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
}

void
VsyncDispatcher::Startup()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  gfxPlatform::GetPlatform()->GetHardwareVsync()->AddVsyncDispatcher(this);
}

void
VsyncDispatcher::Shutdown()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  gfxPlatform::GetPlatform()->GetHardwareVsync()->RemoveVsyncDispatcher(this);
}

void
VsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
#ifdef MOZ_ENABLE_PROFILER_SPS
    if (profiler_is_active()) {
        CompositorParent::PostInsertVsyncProfilerMarker(aVsyncTimestamp);
    }
#endif

  // Notify compositor.
  bool notifiedCompositors = NotifyVsyncObserver(mCompositorObserverLock,
                                                 aVsyncTimestamp,
                                                 mCompositorObserver);

  // Notify input dispatcher.
  // We will dispatch touch event in compositor. If compositor is just be
  // notified at this frame, we don't need to dispatch touch event.
  // Touch events can sometimes start a composite, so make sure we dispatch
  // touches even if we don't composite.
  if (!notifiedCompositors) {
    DispatchTouchEvents(aVsyncTimestamp);
  }

  // Noitfy refresh driver.
  NotifyVsyncObserver(mRefreshDriverObserverLock,
                      aVsyncTimestamp,
                      mRefreshDriverObservers);
}

void
VsyncDispatcher::AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  AddObserver(mCompositorObserverLock, mCompositorObserver, aVsyncObserver);
}

void
VsyncDispatcher::RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  RemoveObserver(mCompositorObserverLock, mCompositorObserver, aVsyncObserver);
}

void
VsyncDispatcher::AddRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver)
{
  AddObserver(mRefreshDriverObserverLock, mRefreshDriverObservers, aVsyncObserver);
}

void
VsyncDispatcher::RemoveRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver)
{
  RemoveObserver(mRefreshDriverObserverLock, mRefreshDriverObservers, aVsyncObserver);
}

void
VsyncDispatcher::DispatchTouchEvents(TimeStamp aVsyncTime)
{
#ifdef MOZ_WIDGET_GONK
  if (gfxPrefs::TouchResampling()) {
    GeckoTouchDispatcher::NotifyVsync(aVsyncTime);
  }
#endif
}

void
VsyncDispatcher::AddObserver(Mutex& aMutex,
                             nsRefPtr<VsyncObserver>& aObserver,
                             VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(aVsyncObserver);
  MutexAutoLock lock(aMutex);

  aObserver = aVsyncObserver;
}

void
VsyncDispatcher::RemoveObserver(Mutex& aMutex,
                                nsRefPtr<VsyncObserver>& aObserver,
                                VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(aVsyncObserver);
  MutexAutoLock lock(aMutex);

  if (aObserver == aVsyncObserver) {
    aObserver = nullptr;
  }
}

void
VsyncDispatcher::AddObserver(Mutex& aMutex,
                             nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
                             VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(aVsyncObserver);
  MutexAutoLock lock(aMutex);

  if (!aObserverList.Contains(aVsyncObserver)) {
    aObserverList.AppendElement(aVsyncObserver);
  }
}

void
VsyncDispatcher::RemoveObserver(Mutex& aMutex,
                                nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
                                VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(aVsyncObserver);
  MutexAutoLock lock(aMutex);

  if (aObserverList.Contains(aVsyncObserver)) {
    aObserverList.RemoveElement(aVsyncObserver);
  }
}

bool
VsyncDispatcher::NotifyVsyncObserver(Mutex& aMutex,
                                     TimeStamp aVsyncTimestamp,
                                     nsRefPtr<VsyncObserver>& aObserver)
{
  MutexAutoLock lock(aMutex);

  if (aObserver) {
    aObserver->NotifyVsync(aVsyncTimestamp);

    return true;
  }

  return false;
}

bool
VsyncDispatcher::NotifyVsyncObserver(Mutex& aMutex,
                                     TimeStamp aVsyncTimestamp,
                                     nsTArray<nsRefPtr<VsyncObserver>>& aObservers)
{
  MutexAutoLock lock(aMutex);

  for (size_t i = 0; i < aObservers.Length(); ++i) {
    aObservers[i]->NotifyVsync(aVsyncTimestamp);
  }

  return !aObservers.IsEmpty();
}

} // namespace mozilla
