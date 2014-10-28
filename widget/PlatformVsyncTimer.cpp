/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformVsyncTimer.h"
#include "mozilla/Assertions.h"
#include "nsTArray.h"

namespace mozilla {

/*static*/ PlatformVsyncTimerFactory::VsyncTimerCreator
PlatformVsyncTimerFactory::mCustomCreator = nullptr;

static PlatformVsyncTimer*
CreateHWTimer(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // TODO: create the hw timer here.

  return timer;
}

static PlatformVsyncTimer*
CreateSWTimer(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // TODO: create the sw timer here.

  return timer;
}

/*static*/ void
PlatformVsyncTimerFactory::SetCustomCreator(VsyncTimerCreator aCreator)
{
  mCustomCreator = aCreator;
}

/*static*/ PlatformVsyncTimer*
PlatformVsyncTimerFactory::Create(VsyncTimerObserver* aObserver)
{
  PlatformVsyncTimer* timer = nullptr;

  // We will use the following sequence to create the timer.
  // 1. custom timer
  // 2. platform HW timer
  // 3. platform SW timer
  nsTArray<VsyncTimerCreator> creator;
  if (mCustomCreator) {
    creator.AppendElement(mCustomCreator);
  }
  creator.AppendElement(CreateHWTimer);
  creator.AppendElement(CreateSWTimer);

  // Iterate the creator chain.
  for (int i = 0; i < creator.Length(); ++i) {
    timer = creator[i](aObserver);
    if (timer && timer->Startup()) {
      break;
    } else {
      // The timer is nullptr or inits failed. Delete the timer here.
      delete timer;
      timer = nullptr;
    }
  }

  // Assert here if here is no timer available.
  MOZ_RELEASE_ASSERT(timer, "No vsync timer implementation.");

  return timer;
}

} // namespace mozilla
