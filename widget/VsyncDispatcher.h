/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_VsyncDispatcher_h
#define mozilla_widget_VsyncDispatcher_h

#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

namespace mozilla {

class VsyncObserver
{
public:
  // Refcounting interface for nsRefPtr<>.
  virtual MozExternalRefCountType AddRef(void) = 0;
  virtual MozExternalRefCountType Release(void) = 0;

  // The method called when a vsync occurs. Return true if some work was done.
  // This function will be call on vsync thread. If observer's thread model is
  // not on vsync thread, it should handle this itself.
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

  // Update the vsync rate for refresh driver observe.
  virtual void UpdateVsyncRate(uint32_t aVsyncRate) { };

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
  // It will be no-op if the observer is already added/removed when we call
  // AddObserver/RemoveObserver.
  // The observer should call remove observer before its dtor.
  // These functions can run at any thread.
  void AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver);

  void AddRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver);

private:
  VsyncDispatcher();
  virtual ~VsyncDispatcher();

  void AddObserver(Mutex& aMutex,
                   nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
                   VsyncObserver* aVsyncObserver);
  void RemoveObserver(Mutex& aMutex,
                      nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
                      VsyncObserver* aVsyncObserver);

  void DispatchTouchEvents(bool aNotifiedCompositors, TimeStamp aVsyncTime);

  // Called on the vsync thread. Returns true if observers were notified.
  bool NotifyVsyncObservers(Mutex& aMutex,
                            TimeStamp aVsyncTimestamp,
                            nsTArray<nsRefPtr<VsyncObserver>>& aObservers);

  // Can have multiple compositors. On desktop, this is 1 compositor per window.
  Mutex mCompositorObserverLock;
  nsTArray<nsRefPtr<VsyncObserver>> mCompositorObservers;

  // Chrome and Content's refresh driver observer list.
  Mutex mRefreshDriverObserverLock;
  nsTArray<nsRefPtr<VsyncObserver>> mRefreshDriverObservers;
};

} // namespace mozilla

#endif // mozilla_widget_VsyncDispatcher_h
