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

// This class provide the registering interface for vsync observer.
class VsyncEventRegistry
{
public:
  virtual uint32_t GetObserverNum() const = 0;

  // Register/Unregister vsync observer.
  // The Register() call is one-shot registry. We only call TickVsync()
  // once per Register(). If observer need another tick, it should call
  // Register() again.
  // All vsync observers should call sync unregister call before they
  // call destructor, so we will not tick the observer after destroyed.
  virtual void Register(VsyncObserver* aVsyncObserver) = 0;
  virtual void Unregister(VsyncObserver* aVsyncObserver, bool aSync = false) = 0;

protected:
  virtual ~VsyncEventRegistry() {}
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
