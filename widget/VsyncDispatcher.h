/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_VsyncDispatcher_h
#define mozilla_widget_VsyncDispatcher_h

#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

namespace mozilla {

class VsyncObserver
{
public:
  // The method called when a vsync occurs. Return true if some work was done.
  // Vsync notifications will occur on the hardware vsync thread
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
};

// VsyncDispatcher is used to dispatch vsync events to the registered observers.
class VsyncDispatcher
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcher);

public:
  // Startup/shutdown vsync dispatcher framework. It should run at Chrome
  // main thread.
  static void Startup();
  static void Shutdown();

  static VsyncDispatcher* GetInstance();

  // Called on the vsync thread when a vsync event occurs.
  void NotifyVsync(TimeStamp aVsyncTimestamp);

  // Return the vsync rate per second.
  uint32_t GetVsyncRate() const ;

  // Add/remove vsync observer for compositor and refresh driver.
  // The observer should call remove observer before its dtor.
  // These function can run at any thread.
  void AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver);

  void AddRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver);

private:
  VsyncDispatcher();
  virtual ~VsyncDispatcher();

  void AddObserver(Mutex& aMutex,
                   nsTArray<VsyncObserver*>& aObserverList,
                   VsyncObserver* aVsyncObserver);
  void RemoveObserver(Mutex& aMutex,
                      nsTArray<VsyncObserver*>& aObserverList,
                      VsyncObserver* aVsyncObserver);

  void DispatchTouchEvents(bool aNotifiedCompositors, TimeStamp aVsyncTime);

  // Called on the vsync thread. Returns true if observers were notified.
  bool NotifyVsyncObservers(Mutex& aMutex,
                            TimeStamp aVsyncTimestamp,
                            nsTArray<VsyncObserver*>& aObservers);

  // Can have multiple compositors. On desktop, this is 1 compositor per window.
  Mutex mCompositorObserverLock;
  nsTArray<VsyncObserver*> mCompositorObservers;

  // Chrome and Content's refresh driver observer list.
  Mutex mRefreshDriverObserverLock;
  nsTArray<VsyncObserver*> mRefreshDriverObservers;
};

} // namespace mozilla

#endif // mozilla_widget_VsyncDispatcher_h
