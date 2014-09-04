/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformVsyncTimer.h"
#include "mozilla/Assertions.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkVsyncTimer.h"
#endif

namespace mozilla {

static PlatformVsyncTimer*
CreateHWTimer(VsyncTimerObserver* aObserver)
{
  // Create the platform dependent vsync timer here
#ifdef MOZ_WIDGET_GONK
  return new GonkVsyncTimer(aObserver);
#endif

  return nullptr;
}

static PlatformVsyncTimer*
CreateSWTimer(VsyncTimerObserver* aObserver)
{
  // Create the sw vsync timer here

  return nullptr;
}

VsyncTimerObserver* PlatformVsyncTimerFactory::mObserver = nullptr;

/*static*/ void
PlatformVsyncTimerFactory::Init(VsyncTimerObserver* aObserver)
{
  mObserver = aObserver;
}

/*static*/ PlatformVsyncTimer*
PlatformVsyncTimerFactory::Create()
{
  MOZ_ASSERT(mObserver, "We should call PlatformVsyncTimerFactory::Init() first.");

  PlatformVsyncTimer* timer = nullptr;

  // We init the hw timer first, and then fallback to sw timer.
  timer = ChooseTimer(CreateHWTimer(mObserver));
  if (!timer){
    timer = ChooseTimer(CreateSWTimer(mObserver));
  }

  // Just assert if here is no matched timer.
  MOZ_RELEASE_ASSERT(timer, "No vsync timer implementation.");

  return timer;
}

/*static*/ PlatformVsyncTimer*
PlatformVsyncTimerFactory::ChooseTimer(PlatformVsyncTimer* aTimer)
{
  if (aTimer) {
    if (!aTimer->Startup()) {
      // Startup timer failed. Delete the timer.
      delete aTimer;
      aTimer = nullptr;
    }
  }

  return aTimer;
}

} // namespace mozilla
