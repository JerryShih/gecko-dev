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
    , mVsyncRate(0)
  {
  }

  virtual ~PlatformVsyncTimer() { }

  // Shutdown the timer.
  virtual void Shutdown() = 0;

  // Enable/disable the vsync event.
  // It will become no functionality after shutdown.
  virtual void Enable(bool aEnable) = 0;

  // Query the vsync timer rate per second.
  // Return 0 if failed.
  virtual uint32_t GetVsyncRate() = 0;

private:
  // Startup the timer.
  // Return true if the timer initials successfully.
  virtual bool Startup() = 0;

protected:
  VsyncTimerObserver* mObserver;
  uint32_t mVsyncRate;
};

} // namespace mozilla

#endif //mozilla_widget_shared_PlatformVsyncTimer_h
