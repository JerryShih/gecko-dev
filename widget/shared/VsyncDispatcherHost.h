/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VsyncDispatcherHost_h
#define mozilla_VsyncDispatcherHost_h

#include "nsTArray.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "VsyncDispatcher.h"

class MessageLoop;

namespace base{
class Thread;
}

namespace mozilla {

class ObserverListHelper;

namespace layers {
class VsyncEventParent;
}

/*
 * The host side vsync dispatcher implementation.
 * We need to implement the platform dependent vsync related function for each
 * platform(ex: StartUpVsyncEvent()).
 */
class VsyncDispatcherHost : public VsyncDispatcher
                          , public CompositorTrigger
                          , public RefreshDriverTrigger
{
  friend class layers::VsyncEventParent;
  friend class ObserverListHelper;

  // We would like to create and delete the VsyncEventChild at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherHost);

public:
  static VsyncDispatcherHost* GetInstance();

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;

  // All vsync observers should call sync unregister call before they
  // call destructor.
  // Enable/disable input dispatcher to do input dispatch at vsync.
  void EnableInputDispatcher();
  void DisableInputDispatcher(bool aSync);

  // Reg/Unregister compositor for vsync composing.
  virtual void RegisterCompositor(VsyncObserver* aCompositor) MOZ_OVERRIDE;
  virtual void UnregisterCompositor(VsyncObserver* aCompositor, bool aSync) MOZ_OVERRIDE;

  // Reg/Unregister refresh driver timer to do tick at vsync.
  virtual void RegisterTimer(VsyncObserver* aTimer) MOZ_OVERRIDE;
  virtual void UnregisterTimer(VsyncObserver* aTimer, bool aSync) MOZ_OVERRIDE;

  // Return the vsync event fps.
  virtual uint32_t GetVsyncRate() MOZ_OVERRIDE;

  // This function is called by vsync event generator.
  // It will post a notify task to vsync dispatcher thread.
  // The timestamp is microsecond.
  void NotifyVsync(int64_t aTimestampUS);

  // Get VsyncDispatcher's message loop
  MessageLoop* GetMessageLoop();

protected:
  VsyncDispatcherHost();
  virtual ~VsyncDispatcherHost();

  bool IsInVsyncDispatcherHostThread();

protected:
  uint32_t mVsyncRate;

private:
  virtual RefreshDriverTrigger* AsRefreshDriverTrigger() MOZ_OVERRIDE;
  virtual CompositorTrigger* AsCompositorTrigger() MOZ_OVERRIDE;

  void CreateVsyncDispatchThread();

  // Startup/shutdown the platform dependent vsync event generator.
  virtual void StartupVsyncEvent() = 0;
  virtual void ShutdownVsyncEvent() = 0;
  virtual void EnableVsyncEvent(bool aEnable) = 0;

  // Set IPC parent. It should be called at vsync dispatcher thread.
  void RegisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent);
  void UnregisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent);

  // Generate frame number and call dispatch event.
  void NotifyVsyncTask(int64_t aTimestampUS);

  // Dispatch vsync to observer
  // This function should run at vsync dispatcher thread
  void DispatchVsyncEvent(int64_t aTimestampUS, uint32_t aFrameNumber);

  // Notify the main thread to handle input event.
  void DispatchInputEvent(int64_t aTimestampUS, uint32_t aFrameNumber);

  // Notify compositor to do compose.
  void Compose(int64_t aTimestampUS, uint32_t aFrameNumber);

  // Tick refresh driver.
  void TickRefreshDriver(int64_t aTimestampUS, uint32_t aFrameNumber);

  // Sent vsync event to all registered content processes
  void NotifyContentProcess(int64_t aTimestampUS, uint32_t aFrameNumber);

  // Return total registered object number.
  int GetVsyncObserverCount() const;

  // Check the observer number in VsyncDispatcher to enable/disable vsync event
  // notification.
  void EnableVsyncNotificationIfhasObserver();

private:
  static scoped_refptr<VsyncDispatcherHost> mVsyncDispatcherHost;

  bool mInited;

  base::Thread* mVsyncDispatchHostThread;
  MessageLoop* mVsyncDispatchHostMessageLoop;

  //TODO: Put other vsync observer list here.
  // Ex: input, compositor and refresh driver.

  // IPC parent
  typedef nsTArray<layers::VsyncEventParent*> VsyncEventParentList;
  // IPC parent
  VsyncEventParentList mVsyncEventParentList;

  //TODO: Put other vsync observer list here.
  // Ex: input, compositor and refresh driver.

  bool mVsyncEventNeeded;
};

} // namespace mozilla

#endif // mozilla_VsyncDispatcherHost_h
