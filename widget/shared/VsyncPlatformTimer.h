/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncPlatformTimer_h
#define mozilla_widget_shared_VsyncPlatformTimer_h

namespace mozilla {

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
// The implementation is at the platform specific folder.
// Please check:
//   widget/android/VsyncPlatformTimerImpl.h
//   widget/gonk/VsyncPlatformTimerImpl.h
//   widget/windows/VsyncPlatformTimerImpl.h
class VsyncPlatformTimer
{
public:
  virtual ~VsyncPlatformTimer() { }

  // We should register the callback before we call startup
  virtual void SetObserver(VsyncTimerObserver* aObserver) = 0;

  // Startup/shutdown the timer. We should only call startup/shutdown once.
  virtual void Startup() = 0;
  virtual void Shutdown() = 0;

  // Enable/disable the vsync event.
  virtual void Enable(bool aEnable) = 0;

  // Query the vsync timer rate per second. It should be called after startup.
  virtual uint32_t GetVsyncRate() = 0;
};

class VsyncPlatformTimerFactory
{
public:
  static VsyncPlatformTimer* GetTimer();
};

} // namespace mozilla

#endif //mozilla_widget_shared_VsyncPlatformTimer_h
