/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformVsyncTimer.h"
#include "mozilla/Assertions.h"
#include "nsTArray.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkVsyncTimer.h"
#endif

namespace mozilla {

/*static*/ PlatformVsyncTimerFactory::VsyncTimerCreator
PlatformVsyncTimerFactory::mCustomCreator = nullptr;

static PlatformVsyncTimer*
CreateHWTimer(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // Create the platform dependent vsync timer here.
#ifdef MOZ_WIDGET_GONK
  timer = new GonkVsyncTimer(aObserver);
#endif

  return timer;
}

static PlatformVsyncTimer*
CreateSWTimer(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // TODO: create the sw timer here.

  return timer;
}

/*static*/ PlatformVsyncTimer*
PlatformVsyncTimerFactory::Create(VsyncTimerObserver* aObserver)
{
  // We will use the following sequence to create the timer.
  // 1. customized timer
  // 2. platform HW timer
  // 3. platform SW timer
  PlatformVsyncTimer* timer = nullptr;
  nsTArray<VsyncTimerCreator> creator;

  if (mCustomCreator) {
    creator.AppendElement(mCustomCreator);
  }
  creator.AppendElement(CreateHWTimer);
  creator.AppendElement(CreateSWTimer);

  // Iterate creator chain.
  for (int i = 0; i < creator.Length(); ++i) {
    timer = creator[i](aObserver);
    if (timer && timer->Startup()) {
      break;
    } else {
      delete timer;
      timer = nullptr;
    }
  }

  // Assert here if here is no timer available.
  MOZ_RELEASE_ASSERT(timer, "No vsync timer implementation.");

  return timer;
}

} // namespace mozilla
