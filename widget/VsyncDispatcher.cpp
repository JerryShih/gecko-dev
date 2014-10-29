/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "gfxPrefs.h"

#ifdef MOZ_WIDGET_GONK
#include "GeckoTouchDispatcher.h"
#include "HwcComposer2D.h"
#endif

using namespace mozilla::layers;

namespace mozilla {

StaticRefPtr<VsyncDispatcher> sVsyncDispatcher;

/*static*/ void
VsyncDispatcher::Startup()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(!sVsyncDispatcher);

  sVsyncDispatcher = new VsyncDispatcher();

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  gfxPrefs::GetSingleton();

  // Get vsync rate from hwc.
  if (gfxPrefs::HardwareVsyncEnabled()) {
    MOZ_ALWAYS_TRUE(HwcComposer2D::GetInstance()->HasHWVsync());
    // Enable the hw vsync event.
    HwcComposer2D::GetInstance()->EnableVsync(true);
  }
#endif
}

/*static*/ void
VsyncDispatcher::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(sVsyncDispatcher);

  sVsyncDispatcher = nullptr;
}

/*static*/ VsyncDispatcher*
VsyncDispatcher::GetInstance()
{
  MOZ_ASSERT(sVsyncDispatcher);

  return sVsyncDispatcher;
}

VsyncDispatcher::VsyncDispatcher()
  : mCompositorObserverLock("CompositorObserverLock")
  , mRefreshDriverObserverLock("RefreshDriverObserverLock")
{
}

VsyncDispatcher::~VsyncDispatcher()
{
}

uint32_t
VsyncDispatcher::GetVsyncRate() const
{
  // Currently we only have GONK impl, return 60 for default value.
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  return HwcComposer2D::GetInstance()->GetHWVsyncRate();
#else
  return 60;
#endif
}

void
VsyncDispatcher::DispatchTouchEvents(bool aNotifiedCompositors, TimeStamp aVsyncTime)
{
  // Touch events can sometimes start a composite, so make sure we dispatch touches
  // even if we don't composite
#ifdef MOZ_WIDGET_GONK
  if (!aNotifiedCompositors && gfxPrefs::TouchResampling()) {
    GeckoTouchDispatcher::NotifyVsync(aVsyncTime);
  }
#endif
}

void
VsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // Notify compositor.
  bool notifiedCompositors = NotifyVsyncObservers(mCompositorObserverLock,
                                                  aVsyncTimestamp,
                                                  mCompositorObservers);

  // Notify input dispatcher.
  DispatchTouchEvents(notifiedCompositors, aVsyncTimestamp);

  // Noitfy refresh driver.
  NotifyVsyncObservers(mRefreshDriverObserverLock,
                       aVsyncTimestamp,
                       mRefreshDriverObservers);
}

bool
VsyncDispatcher::NotifyVsyncObservers(Mutex& aMutex,
                                      TimeStamp aVsyncTimestamp,
                                      nsTArray<VsyncObserver*>& aObservers)
{
  MutexAutoLock lock(aMutex);

  for (size_t i = 0; i < aObservers.Length(); ++i) {
    aObservers[i]->NotifyVsync(aVsyncTimestamp);
 }
 return !aObservers.IsEmpty();
}

void
VsyncDispatcher::AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  AddObserver(mCompositorObserverLock, mCompositorObservers, aVsyncObserver);
}

void
VsyncDispatcher::RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  RemoveObserver(mCompositorObserverLock, mCompositorObservers, aVsyncObserver);
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
VsyncDispatcher::AddObserver(Mutex& aMutex,
                             nsTArray<VsyncObserver*>& aObserverList,
                             VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(aMutex);

  if (!aObserverList.Contains(aVsyncObserver)) {
    aObserverList.AppendElement(aVsyncObserver);
  }
}

void
VsyncDispatcher::RemoveObserver(Mutex& aMutex,
                                nsTArray<VsyncObserver*>& aObserverList,
                                VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(aMutex);

  if (aObserverList.Contains(aVsyncObserver)) {
    aObserverList.RemoveElement(aVsyncObserver);
  }
}

} // namespace mozilla
