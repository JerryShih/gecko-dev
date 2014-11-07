/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/layers/CompositorParent.h"
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

  // Init vsync event
  if (gfxPrefs::HardwareVsyncEnabled()) {
    MOZ_ALWAYS_TRUE(HwcComposer2D::GetInstance()->HasHWVsync());
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
  {
    MutexAutoLock lock(mCompositorObserverLock);
    mCompositorObservers.Clear();
  }
  {
    MutexAutoLock lock(mRefreshDriverObserverLock);
    mRefreshDriverObservers.Clear();
  }
}

uint32_t
VsyncDispatcher::GetVsyncRate()
{
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
  bool notifiedCompositors = false;
  notifiedCompositors = NotifyVsyncObservers(mCompositorObserverLock,
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
VsyncDispatcher::NotifyVsyncObservers(Mutex& aMutex, TimeStamp aVsyncTimestamp, nsTArray<nsRefPtr<VsyncObserver>>& aObservers)
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
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  MutexAutoLock lock(mCompositorObserverLock);

  if (!mCompositorObservers.Contains(aVsyncObserver)) {
    mCompositorObservers.AppendElement(aVsyncObserver);
  }
}

void
VsyncDispatcher::RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  MutexAutoLock lock(mCompositorObserverLock);

  if (mCompositorObservers.Contains(aVsyncObserver)) {
    mCompositorObservers.RemoveElement(aVsyncObserver);
  } else {
    NS_WARNING("Could not delete a compositor vsync observer\n");
  }
}

void
VsyncDispatcher::AddRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mRefreshDriverObserverLock);

  if (!mRefreshDriverObservers.Contains(aVsyncObserver)) {
    mRefreshDriverObservers.AppendElement(aVsyncObserver);
  }
}

void
VsyncDispatcher::RemoveRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mRefreshDriverObserverLock);

  if (mRefreshDriverObservers.Contains(aVsyncObserver)) {
    mRefreshDriverObservers.RemoveElement(aVsyncObserver);
  }
}

} // namespace mozilla
