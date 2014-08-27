/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncPlatformTimer.h"
#include "mozilla/Assertions.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkVsyncTimer.h"
#endif

namespace mozilla {

/*static*/ VsyncPlatformTimer*
VsyncPlatformTimerFactory::GetTimer()
{
  // We should crete the platform dependent vsync timer here
#ifdef MOZ_WIDGET_GONK
  return new GonkVsyncTimer;
#endif

  // Just crash if here is no matched timer.
  MOZ_CRASH("No vsync timer implementation.");
}

} // namespace mozilla
