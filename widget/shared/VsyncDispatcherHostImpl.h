/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcherHostImpl_h
#define mozilla_widget_shared_VsyncDispatcherHostImpl_h

#include "mozilla/TimeStamp.h"
#include "mozilla/StaticPtr.h"
#include "nsTArray.h"
#include "PlatformVsyncTimer.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "VsyncDispatcher.h"

class MessageLoop;

namespace base{
class Thread;
}

namespace mozilla {

class ObserverListHelper;

// Registries
class VsyncEventRegistryHost;
class CompositorRegistryHost;
class InputDispatcherRegistryHost;
class RefreshDriverRegistryHost;

// The host side vsync dispatcher implementation.
class VsyncDispatcherHostImpl MOZ_FINAL : public VsyncDispatcher
                                        , public VsyncDispatcherHost
                                        , public VsyncTimerObserver
{
  // We would like to create and delete the VsyncDispatcherHostImpl at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherHostImpl);

public:
  static VsyncDispatcherHostImpl* GetInstance();

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;

  // Get VsyncDispatcher's message loop
  virtual MessageLoop* GetMessageLoop() MOZ_OVERRIDE;

  // Check the observer number in VsyncDispatcher to enable/disable vsync event
  // notification.
  void EnableVsyncNotificationIfhasObserver();

  bool IsInVsyncDispatcherThread() const;

private:
  VsyncDispatcherHostImpl();
  virtual ~VsyncDispatcherHostImpl();

  virtual VsyncDispatcherHost* AsVsyncDispatcherHost() MOZ_OVERRIDE;

  // This function is called by vsync event generator.
  // It will post a notify task to vsync dispatcher thread.
  // The timestamp is microsecond.
  virtual void NotifyVsync(TimeStamp aTimestamp, int64_t aTimestampJS) MOZ_OVERRIDE;

  // Set IPC parent. It should be called at vsync dispatcher thread.
  virtual void RegisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) MOZ_OVERRIDE;
  virtual void UnregisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) MOZ_OVERRIDE;

  // Return the vsync event fps.
  virtual uint32_t GetVsyncRate() const MOZ_OVERRIDE;

  virtual VsyncEventRegistry* GetInputDispatcherRegistry() MOZ_OVERRIDE;
  virtual VsyncEventRegistry* GetRefreshDriverRegistry() MOZ_OVERRIDE;
  virtual VsyncEventRegistry* GetCompositorRegistry() MOZ_OVERRIDE;

  void CreateVsyncDispatchThread();

  // Generate frame number and call dispatch event.
  void NotifyVsyncTask(TimeStamp aTimestamp,int64_t aTimestampUS, uint64_t aFrameNumber);

  // Dispatch vsync to observer
  // This function should run at vsync dispatcher thread
  void DispatchVsyncEvent();

  // Notify the main thread to handle input event.
  void DispatchInputEvent();

  // Notify compositor to do compose.
  void DispatchCompose();

  void TickRefreshDriver();

  // Sent vsync event to all registered content processes
  void NotifyContentProcess();

  // Return total registered object number.
  int GetVsyncObserverCount();

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

  // Registries
  InputDispatcherRegistryHost* mInputDispatcher;
  RefreshDriverRegistryHost* mRefreshDriver;
  CompositorRegistryHost* mCompositor;

  PlatformVsyncTimer* mTimer;

  // Vsync event tick timestamp.
  TimeStamp mCurrentTimestamp;
  // Vsync event tick timestamp in JS.
  int64_t mCurrentTimestampJS;
  // Vsync event current frame number.
  uint64_t mCurrentFrameNumber;
  // Monotonic increased frame number.
  uint64_t mVsyncFrameNumber;
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcherHostImpl_h
