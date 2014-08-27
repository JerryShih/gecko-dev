/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcherHostImpl_h
#define mozilla_widget_shared_VsyncDispatcherHostImpl_h

#include "mozilla/StaticPtr.h"
#include "nsTArray.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "VsyncDispatcher.h"
#include "VsyncPlatformTimer.h"

class MessageLoop;

namespace base{
class Thread;
}

namespace mozilla {

class ObserverListHelper;

class RefreshDriverRegistryHost;
class CompositorRegistryHost;

class VsyncPlatformTimer;

// The host side vsync dispatcher implementation.
class VsyncDispatcherHostImpl MOZ_FINAL : public VsyncDispatcherHost
                                        , public VsyncTimerObserver
{
  friend class ObserverListHelper;
  friend class VsyncEventRegistryHost;

  // We would like to create and delete the VsyncDispatcherHostImpl at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherHostImpl);

public:
  static VsyncDispatcherHostImpl* GetInstance();

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;

private:
  VsyncDispatcherHostImpl();
  virtual ~VsyncDispatcherHostImpl();

  virtual VsyncDispatcherHost* AsVsyncDispatcherHost() MOZ_OVERRIDE;

  // This function is called by vsync event generator.
  // It will post a notify task to vsync dispatcher thread.
  // The timestamp is microsecond.
  virtual void NotifyVsync(int64_t aTimestampUS) MOZ_OVERRIDE;

  // Enable/disable input dispatcher to do input dispatch at vsync.
  virtual void EnableInputDispatcher() MOZ_OVERRIDE;
  virtual void DisableInputDispatcher(bool aSync) MOZ_OVERRIDE;

  // Set IPC parent. It should be called at vsync dispatcher thread.
  virtual void RegisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) MOZ_OVERRIDE;
  virtual void UnregisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) MOZ_OVERRIDE;

  // Return the vsync event fps.
  virtual uint32_t GetVsyncRate() const MOZ_OVERRIDE;

  // Get VsyncDispatcher's message loop
  virtual MessageLoop* GetMessageLoop() MOZ_OVERRIDE;

  virtual VsyncEventRegistry* GetRefreshDriverRegistry() MOZ_OVERRIDE;
  virtual VsyncEventRegistry* GetCompositorRegistry() MOZ_OVERRIDE;

  void CreateVsyncDispatchThread();

  // Generate frame number and call dispatch event.
  void NotifyVsyncTask(int64_t aTimestampUS);

  // Dispatch vsync to observer
  // This function should run at vsync dispatcher thread
  void DispatchVsyncEvent();

  // Notify the main thread to handle input event.
  void DispatchInputEvent();

  // Notify compositor to do compose.
  void Compose();

  // Tick refresh driver.
  void TickRefreshDriver();

  // Sent vsync event to all registered content processes
  void NotifyContentProcess();

  // Return total registered object number.
  int GetVsyncObserverCount();

  // Check the observer number in VsyncDispatcher to enable/disable vsync event
  // notification.
  void EnableVsyncNotificationIfhasObserver();

  bool IsInVsyncDispatcherThread();

private:
  static StaticRefPtr<VsyncDispatcherHostImpl> sVsyncDispatcherHost;

  uint32_t mVsyncRate;

  bool mInited;
  bool mVsyncEventNeeded;

  base::Thread* mVsyncDispatchHostThread;
  MessageLoop* mVsyncDispatchHostMessageLoop;

  // IPC parent
  typedef nsTArray<layers::VsyncEventParent*> VsyncEventParentList;
  VsyncEventParentList mVsyncEventParentList;

  RefreshDriverRegistryHost* mRefreshDriver;

  CompositorRegistryHost* mCompositor;

  VsyncPlatformTimer* mTimer;

  // Vsync event tick timestamp.
  int64_t mCurrentTimestampUS;
  // Monotonic increased frame number.
  uint64_t mCurrentFrameNumber;
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcherHostImpl_h
