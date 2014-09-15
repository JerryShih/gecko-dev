/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformVsyncTimer.h"
#include "mozilla/Assertions.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkVsyncTimer.h"
#endif

namespace mozilla {

/*static*/ PlatformVsyncTimer*
PlatformVsyncTimerFactory::CreateHWTimer(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // Create the platform dependent vsync timer here.
#ifdef MOZ_WIDGET_GONK
  timer = new GonkVsyncTimer(aObserver);
#endif

  if (timer) {
    if (!timer->Startup()) {
      // Init hw timer failed.
      // Delete the allocated timer.
      delete timer;
      timer = nullptr;
    }
  }

  return timer;
}

/*static*/ PlatformVsyncTimer*
PlatformVsyncTimerFactory::CreateSWTimer(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // TODO: create the sw timer here.

  if (timer) {
    // SW timer should not init failed.
    MOZ_RELEASE_ASSERT(timer->Startup());
  }

  return timer;
}

/*static*/ PlatformVsyncTimer*
PlatformVsyncTimerFactory::Create(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // We init the hw timer first, and then fallback to sw timer.
  timer = CreateHWTimer(aObserver);
  if (!timer){
    timer = CreateSWTimer(aObserver);
  }

  // Assert here if here is no timer available.
  MOZ_RELEASE_ASSERT(timer, "No vsync timer implementation.");

  return timer;
}

} // namespace mozilla
