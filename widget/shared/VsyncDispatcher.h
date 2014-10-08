/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcher_h
#define mozilla_widget_shared_VsyncDispatcher_h

#include "mozilla/StaticPtr.h"
#include "nsArray.h"
#include <utils/Timers.h>
#include "mozilla/Mutex.h"
class MessageLoop;

namespace mozilla {
class TimeStamp;

class VsyncObserver
{
public:
  // The vsync-aligned task. Return true if there has a task ticked.
  virtual bool NotifyVsync (TimeStamp aVsyncTimestamp) = 0;

protected:
  virtual ~VsyncObserver() { }
};

// VsyncDispatcher is used to dispatch vsync events to the registered observers.
class VsyncDispatcher
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncDispatcher)

public:
  static VsyncDispatcher* GetInstance();

  // Vsync event rate per second.
  uint32_t GetVsyncRate();
  void NotifyVsync(TimeStamp aVsyncTimestamp, nsecs_t aAndroidVsyncTime);
  void AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver);

private:
  void NotifyVsync(TimeStamp aVsyncTimestamp, nsTArray<VsyncObserver*>& aObservers);
  VsyncDispatcher();
  virtual ~VsyncDispatcher();

  // Can have multiple compositors. On desktop, this is 1 compositor per window
  Mutex mCompositorObserverLock;
  nsTArray<VsyncObserver*> mCompositorObservers;
  static StaticRefPtr<VsyncDispatcher> sVsyncDispatcher;
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcher_h
