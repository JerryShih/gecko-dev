/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VsyncDispatcherClient_h
#define mozilla_VsyncDispatcherClient_h

#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "VsyncDispatcher.h"

namespace mozilla {

class ObserverListHelper;

namespace layers {
class VsyncEventChild;
}

/*
 * The client side vsync dispatcher implementation.
 *
 * We use the main thread as the VsyncDispatcherClient work thread, and all
 * user should use VsyncDispatcherClient at the main thread.
 */
class VsyncDispatcherClient MOZ_FINAL : public VsyncDispatcher
                                      , public RefreshDriverTrigger
{
  friend class layers::VsyncEventChild;
  friend class ObserverListHelper;

  // We would like to create and delete the VsyncEventChild at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherClient);

public:
  static VsyncDispatcherClient* GetInstance();

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;

  void SetVsyncRate(uint32_t aVsyncRate);
  virtual uint32_t GetVsyncRate() MOZ_OVERRIDE;

  // Register refresh driver timer to do tick at vsync.
  // We should call sync unregister before observer call destructor.
  virtual void RegisterTimer(VsyncObserver* aTimer) MOZ_OVERRIDE;
  virtual void UnregisterTimer(VsyncObserver* aTimer, bool aSync) MOZ_OVERRIDE;

private:
  VsyncDispatcherClient();
  virtual ~VsyncDispatcherClient();

  virtual RefreshDriverTrigger* AsRefreshDriverTrigger() MOZ_OVERRIDE;

  bool IsInVsyncDispatcherClientThread();

  // Set IPC child. It should be called at vsync dispatcher thread.
  void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild);

  // Dispatch vsync to observer
  // This function should run at vsync dispatcher thread
  void DispatchVsyncEvent(int64_t aTimestampUS, uint32_t aFrameNumber);

  // Tick refresh driver.
  void TickRefreshDriver(int64_t aTimestampUS, uint32_t aFrameNumber);

  // Return total registered object number.
  int GetVsyncObserverCount() const;

  void EnableVsyncEvent(bool aEnable);

  // Check the observer number in VsyncDispatcher to enable/disable vsync event
  // notification.
  void EnableVsyncNotificationIfhasObserver();

private:
  static scoped_refptr<VsyncDispatcherClient> mVsyncDispatcherClient;

  uint32_t mVsyncRate;

  bool mInited;

  // IPC child
  layers::VsyncEventChild* mVsyncEventChild;

  //TODO: Put refresh driver list here.

  bool mVsyncEventNeeded;
};

} // namespace mozilla

#endif // mozilla_VsyncDispatcherClient_h
