/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_PlatformVsyncTimer_h
#define mozilla_widget_PlatformVsyncTimer_h

#include "mozilla/TimeStamp.h"

namespace mozilla {

class PlatformVsyncTimer;
class PlatformVsyncTimerFactory;
class VsyncTimerObserver;

class PlatformVsyncTimerFactory
{
public:
  typedef PlatformVsyncTimer* (*VsyncTimerCreator)(VsyncTimerObserver *aObserver);

  // User can set the custom timer creator. If the custom creator has set,
  // the factory will use this creator to create the vsync timer.
  // Set nullptr to use the original factory creator.
  static void SetCustomCreator(VsyncTimerCreator aCreator);

  // Create the platform dependent vsync timer.
  static PlatformVsyncTimer* Create(VsyncTimerObserver* aObserver);

private:
  static VsyncTimerCreator mCustomCreator;
};

// This is the base class which will be notified for every vsync event.
class VsyncTimerObserver
{
public:
  // Notify the the observer that there has one vsync event.
  virtual void NotifyVsync(TimeStamp aTimestamp) = 0;

protected:
  ~VsyncTimerObserver() { };
};

// A cross-platform vsync timer that provides the vsync events for
// VsyncTimerObserver.
// We need to call Enable(true) to start the timer at first.
//
// The implementation is at the platform specific folder.
// Please check:
//   widget/android/*VsyncTimer.h
//   widget/gonk/*VsyncTimer.h
//   widget/windows/*VsyncTimer.h
class PlatformVsyncTimer
{
  friend class PlatformVsyncTimerFactory;

public:
  PlatformVsyncTimer(VsyncTimerObserver* aObserver)
    : mObserver(aObserver)
  {
  }

  virtual ~PlatformVsyncTimer() { }

  // Shutdown the timer.
  virtual void Shutdown() = 0;

  // Enable/disable the vsync event.
  // It will become no functionality after shutdown.
  virtual void Enable(bool aEnable) = 0;

private:
  // Startup the timer. This function is called by PlatformVsyncTimerFactory to
  // check the timer status. Return true if the timer initials successfully.
  virtual bool Startup() = 0;

protected:
  VsyncTimerObserver* mObserver;
};

} // namespace mozilla

#endif //mozilla_widget_PlatformVsyncTimer_h
