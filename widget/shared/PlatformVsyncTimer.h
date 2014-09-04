/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_PlatformVsyncTimer_h
#define mozilla_widget_shared_PlatformVsyncTimer_h

namespace mozilla {

class PlatformVsyncTimer;
class PlatformVsyncTimerFactory;
class VsyncTimerObserver;

class PlatformVsyncTimerFactory
{
public:
  static PlatformVsyncTimer* Create(VsyncTimerObserver* aObserver);

private:
  static PlatformVsyncTimer* CreateHWTimer(VsyncTimerObserver* aObserver);
  static PlatformVsyncTimer* CreateSWTimer(VsyncTimerObserver* aObserver);
};

// This is the base class which will be notified for every vsync event.
class VsyncTimerObserver
{
public:
  // Notify the the observer that there has one vsync event. the Time timestamp
  // is microsecond.
  virtual void NotifyVsync(int64_t aTimestampUS) = 0;

protected:
  ~VsyncTimerObserver() { };
};

// Platform dependent vsync timer. It will call the VsyncTimerCallback when the
// vsync event comes.
// We need to call Enable(true) to start the timer at first.
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
    , mVsyncRate(0)
  {
  }

  virtual ~PlatformVsyncTimer() { }

  // Shutdown the timer. After shutdown, the Enable() function is invalid.
  virtual void Shutdown() = 0;

  // Enable/disable the vsync event.
  virtual void Enable(bool aEnable) = 0;

  // Query the vsync timer rate per second. It will return 0 if the timer is
  // invalid.
  virtual uint32_t GetVsyncRate() = 0;

private:
  // Startup the timer. It will return false if we can't init the timer.
  virtual bool Startup() = 0;

protected:
  VsyncTimerObserver* mObserver;
  uint32_t mVsyncRate;
};

} // namespace mozilla

#endif //mozilla_widget_shared_PlatformVsyncTimer_h
