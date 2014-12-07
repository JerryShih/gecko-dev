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

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
};

// VsyncDispatcher is used to dispatch vsync events to the registered observers.
// It only run at Chrome process.
class VsyncDispatcher
{
   NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcher)

public:
  VsyncDispatcher();

  void Startup();
  void Shutdown();

  // Called on the vsync thread when a vsync event occurs.
  // The aVsyncTimestamp can mean different things depending on the platform:
  // b2g - The vsync timestamp of the previous frame that was just displayed
  // OSX - The vsync timestamp of the upcoming frame
  // TODO: Windows / Linux. DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
  // Android: TODO
  void NotifyVsync(TimeStamp aVsyncTimestamp);

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
  virtual ~VsyncDispatcher();

  void AddObserver(Mutex& aMutex,
                   nsRefPtr<VsyncObserver>& aObserver,
                   VsyncObserver* aVsyncObserver);
  void RemoveObserver(Mutex& aMutex,
                      nsRefPtr<VsyncObserver>& aObserver,
                      VsyncObserver* aVsyncObserver);
  void AddObserver(Mutex& aMutex,
                   nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
                   VsyncObserver* aVsyncObserver);
  void RemoveObserver(Mutex& aMutex,
                      nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
                      VsyncObserver* aVsyncObserver);

  void DispatchTouchEvents(TimeStamp aVsyncTime);

  // Called on the vsync thread. Returns true if observers were notified.
  bool NotifyVsyncObserver(Mutex& aMutex,
                           TimeStamp aVsyncTimestamp,
                           nsRefPtr<VsyncObserver>& aObserver);
  bool NotifyVsyncObserver(Mutex& aMutex,
                           TimeStamp aVsyncTimestamp,
                           nsTArray<nsRefPtr<VsyncObserver>>& aObservers);

  // We only have one compositor per window.
  Mutex mCompositorObserverLock;
  nsRefPtr<VsyncObserver> mCompositorObserver;

  // Chrome and Content's refresh driver observer list.
  Mutex mRefreshDriverObserverLock;
  nsTArray<nsRefPtr<VsyncObserver>> mRefreshDriverObservers;
}; // VsyncDispatcher

} // namespace mozilla

#endif // mozilla_widget_VsyncDispatcher_h
