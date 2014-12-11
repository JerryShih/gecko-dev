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

namespace layout {
class VsyncEventParent;
class VsyncEventChild;
}

class VsyncObserver
{
public:
  // Refcounting interface for nsRefPtr<>.
  virtual MozExternalRefCountType AddRef(void) = 0;
  virtual MozExternalRefCountType Release(void) = 0;

  // The method called when a vsync occurs. Return true if some work was done.
  // This function will be call on vsync thread at chrome and on PVsyncEvent
  // thread at content. If observer's thread model is different, it should
  // handle this itself.
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
};

//// VsyncDispatcher is used to dispatch vsync events to the registered observers.
//// It only run at Chrome process.
//class VsyncDispatcher
//{
//   NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcher)
//
//public:
//  VsyncDispatcher();
//
//  void Startup();
//  void Shutdown();
//
//  // Called on the vsync thread when a vsync event occurs.
//  // The aVsyncTimestamp can mean different things depending on the platform:
//  // b2g - The vsync timestamp of the previous frame that was just displayed
//  // OSX - The vsync timestamp of the upcoming frame
//  // TODO: Windows / Linux. DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
//  // Android: TODO
//  void NotifyVsync(TimeStamp aVsyncTimestamp);
//
//  // Add/remove vsync observer for compositor and refresh driver.
//  // It will be no-op if the observer is already added/removed when we call
//  // AddObserver/RemoveObserver.
//  // The observer should call remove observer before its dtor.
//  // These functions can run at any thread.
//  void AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
//  void RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
//
//  void AddRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver);
//  void RemoveRefreshDriverVsyncObserver(VsyncObserver* aVsyncObserver);
//
//private:
//  virtual ~VsyncDispatcher();
//
//  void AddObserver(Mutex& aMutex,
//                   nsRefPtr<VsyncObserver>& aObserver,
//                   VsyncObserver* aVsyncObserver);
//  void RemoveObserver(Mutex& aMutex,
//                      nsRefPtr<VsyncObserver>& aObserver,
//                      VsyncObserver* aVsyncObserver);
//  void AddObserver(Mutex& aMutex,
//                   nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
//                   VsyncObserver* aVsyncObserver);
//  void RemoveObserver(Mutex& aMutex,
//                      nsTArray<nsRefPtr<VsyncObserver>>& aObserverList,
//                      VsyncObserver* aVsyncObserver);
//
//  void DispatchTouchEvents(TimeStamp aVsyncTime);
//
//  // Called on the vsync thread. Returns true if observers were notified.
//  bool NotifyVsyncObserver(Mutex& aMutex,
//                           TimeStamp aVsyncTimestamp,
//                           nsRefPtr<VsyncObserver>& aObserver);
//  bool NotifyVsyncObserver(Mutex& aMutex,
//                           TimeStamp aVsyncTimestamp,
//                           nsTArray<nsRefPtr<VsyncObserver>>& aObservers);
//
//  // We only have one compositor per window.
//  Mutex mCompositorObserverLock;
//  nsRefPtr<VsyncObserver> mCompositorObserver;
//
//  // Chrome and Content's refresh driver observer list.
//  Mutex mRefreshDriverObserverLock;
//  nsTArray<nsRefPtr<VsyncObserver>> mRefreshDriverObservers;
//};

class VsyncDispatcherBase
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherBase)

public:
  VsyncDispatcherBase();

  // Called on the vsync thread when a vsync event occurs.
  // The aVsyncTimestamp can mean different things depending on the platform:
  //   b2g - The vsync timestamp of the previous frame that was just displayed
  //   OSX - The vsync timestamp of the upcoming frame
  //
  // TODO: Windows / Linux / Android.
  // DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
  virtual void NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

  // Add/remove vsync observer for refresh driver.
  // See also VsyncDispatcherParent::AddCompositorVsyncObserver().
  void AddRefreshDriverTimerObserver(VsyncObserver* aVsyncObserver);
  void RemoveRefreshDriverTimerObserver(VsyncObserver* aVsyncObserver);

protected:
  virtual ~VsyncDispatcherBase();

  template<typename ObserverType>
  void AddObserver(Mutex& aMutex,
                   nsRefPtr<ObserverType>& aObserver,
                   ObserverType* aVsyncObserver)
  {
    MOZ_ASSERT(aVsyncObserver);
    MutexAutoLock lock(aMutex);

    aObserver = aVsyncObserver;
  }
  template<typename ObserverType>
  void RemoveObserver(Mutex& aMutex,
                      nsRefPtr<ObserverType>& aObserver,
                      ObserverType* aVsyncObserver)
  {
    MOZ_ASSERT(aVsyncObserver);
    MutexAutoLock lock(aMutex);

    if (aObserver == aVsyncObserver) {
      aObserver = nullptr;
    }
  }
  template<typename ObserverType>
  void AddObserver(Mutex& aMutex,
                   nsTArray<nsRefPtr<ObserverType>>& aObserverList,
                   ObserverType* aVsyncObserver)
  {
    MOZ_ASSERT(aVsyncObserver);
    MutexAutoLock lock(aMutex);

    if (!aObserverList.Contains(aVsyncObserver)) {
      aObserverList.AppendElement(aVsyncObserver);
    }
  }
  template<typename ObserverType>
  void RemoveObserver(Mutex& aMutex,
                      nsTArray<nsRefPtr<ObserverType>>& aObserverList,
                      ObserverType* aVsyncObserver)
  {
    MOZ_ASSERT(aVsyncObserver);
    MutexAutoLock lock(aMutex);

    if (aObserverList.Contains(aVsyncObserver)) {
      aObserverList.RemoveElement(aVsyncObserver);
    }
  }

  // Refresh driver observer list.
  Mutex mRefreshDriverTimerObserverLock;
  nsTArray<nsRefPtr<VsyncObserver>> mRefreshDriverTimerObservers;
};

// ChromeVsyncDispatcher is used to dispatch vsync events to the registered
// observers and the cross process child side observers.
// It only runs at chrome process.
class ChromeVsyncDispatcher MOZ_FINAL : public VsyncDispatcherBase
{
  typedef mozilla::layout::VsyncEventParent VsyncEventParent;

public:
  ChromeVsyncDispatcher();

  void Startup();
  void Shutdown();

  // Called on the vsync thread when a vsync event occurs.
  // The aVsyncTimestamp can mean different things depending on the platform:
  //   b2g - The vsync timestamp of the previous frame that was just displayed
  //   OSX - The vsync timestamp of the upcoming frame
  //
  // TODO: Windows / Linux. DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
  // Android: TODO
  virtual void NotifyVsync(TimeStamp aVsyncTimestamp) MOZ_OVERRIDE;

  // All add/remove will be no-op if the observer is already added/removed.
  // The observer and actor should call remove before its dtor.
  // These functions can run at any thread.
  //
  // Add/remove compositor observer.
  void AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  // Add/remove VsyncEventParent actor.
  void AddVsyncEventParent(VsyncEventParent* aVsyncEventParent);
  void RemoveVsyncEventParent(VsyncEventParent* aVsyncEventParent);

private:
  virtual ~ChromeVsyncDispatcher();

  void DispatchTouchEvents(TimeStamp aVsyncTime);

  // We only have one compositor per window.
  Mutex mCompositorObserverLock;
  nsRefPtr<VsyncObserver> mCompositorObserver;

  // VsyncEventParent ipc actor list.
  Mutex mVsyncEventParentLock;
  nsTArray<nsRefPtr<VsyncEventParent>> mVsyncEventParents;
};

// ContentVsyncDispatcher is used to dispatch vsync events to the registered
// observers at content process. It uses PVsyncEvent ipc protocol to connect
// with chrome.
class ContentVsyncDispatcher MOZ_FINAL : public VsyncDispatcherBase
{
  typedef mozilla::layout::VsyncEventChild VsyncEventChild;

public:
  explicit ContentVsyncDispatcher(VsyncEventChild* aVsyncEventChild);

  // Called on the PVsyncEvent ipc thread when a vsync event occurs.
  // Please check VsyncDispatcherParent::NotifyVsync().
  virtual void NotifyVsync(TimeStamp aVsyncTimestamp) MOZ_OVERRIDE;

private:
  virtual ~ContentVsyncDispatcher();

  // VsyncEventChild ipc actor.
  nsRefPtr<VsyncEventChild> mVsyncEventChild;
};

} // namespace mozilla

#endif // mozilla_widget_VsyncDispatcher_h
