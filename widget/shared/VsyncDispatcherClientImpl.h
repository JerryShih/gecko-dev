/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcherClientImpl_h
#define mozilla_widget_shared_VsyncDispatcherClientImpl_h

#include "mozilla/StaticPtr.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "VsyncDispatcher.h"

namespace mozilla {

class ObserverListHelper;
class RefreshDriverRegistryClient;

// The client side vsync dispatcher implementation.
// We use the main thread as the VsyncDispatcherClient work thread, and all
// user should use VsyncDispatcherClient at the main thread.
class VsyncDispatcherClientImpl MOZ_FINAL : public VsyncDispatcherClient
{
  friend class ObserverListHelper;
  friend class RefreshDriverRegistryClient;

  // We would like to create and delete the VsyncDispatcherClientImpl at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherClientImpl);

public:
  static VsyncDispatcherClientImpl* GetInstance();

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;

private:
  VsyncDispatcherClientImpl();
  virtual ~VsyncDispatcherClientImpl();

  virtual VsyncEventRegistry* GetRefreshDriverRegistry() MOZ_OVERRIDE;

  virtual void SetVsyncRate(uint32_t aVsyncRate) MOZ_OVERRIDE;
  virtual uint32_t GetVsyncRate() const MOZ_OVERRIDE;

  virtual VsyncDispatcherClient* AsVsyncDispatcherClient() MOZ_OVERRIDE;

  // Set IPC child. It should be called at vsync dispatcher thread.
  virtual void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild) MOZ_OVERRIDE;

  bool IsInVsyncDispatcherThread();

  // Dispatch vsync to observer
  // This function should run at vsync dispatcher thread
  void DispatchVsyncEvent(int64_t aTimestampUS, uint64_t aFrameNumber);

  void TickRefreshDriver();

  // Return total registered object number.
  int GetVsyncObserverCount();

  void EnableVsyncEvent(bool aEnable);

  // Check the observer number in VsyncDispatcher to enable/disable vsync event
  // notification.
  void EnableVsyncNotificationIfhasObserver();

private:
  static StaticRefPtr<VsyncDispatcherClientImpl> sVsyncDispatcherClient;

  uint32_t mVsyncRate;
  bool mInited;
  bool mVsyncEventNeeded;

  // IPC child
  layers::VsyncEventChild* mVsyncEventChild;

  // Refresh driver
  RefreshDriverRegistryClient* mRefreshDriver;

  // Vsync event tick timestamp.
  int64_t mCurrentTimestampUS;
  // Monotonic increased frame number.
  uint64_t mCurrentFrameNumber;
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcherClientImpl_h
