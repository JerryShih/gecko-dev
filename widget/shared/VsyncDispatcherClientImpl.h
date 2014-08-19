/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcherClientImpl_h
#define mozilla_widget_shared_VsyncDispatcherClientImpl_h

#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/StaticPtr.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "VsyncDispatcher.h"

namespace mozilla {

class RefreshDriverRegistry;

// The client side vsync dispatcher implementation.
// We use the main thread as the VsyncDispatcherClient work thread, and all
// user should use VsyncDispatcherClient at the main thread.
class VsyncDispatcherClientImpl MOZ_FINAL : public VsyncDispatcher
                                          , public VsyncDispatcherClient
{
  // We would like to create and delete the VsyncDispatcherClientImpl at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherClientImpl);

public:
  static VsyncDispatcherClientImpl* GetInstance();
  VsyncDispatcherClientImpl();

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;

  virtual uint32_t GetVsyncRate() const MOZ_OVERRIDE;

  // This function will be called when the registry need vsync tick.
  virtual void VsyncTickNeeded() MOZ_OVERRIDE;

  virtual bool IsInVsyncDispatcherThread() const MOZ_OVERRIDE;

  virtual VsyncDispatcherClient* AsVsyncDispatcherClient() MOZ_OVERRIDE;

  virtual VsyncEventRegistry* GetRefreshDriverRegistry() MOZ_OVERRIDE;

  // Dispatch vsync to observer
  // This function should run at vsync dispatcher thread
  void NotifyVsync(TimeStamp aTimestamp, uint64_t aFrameNumber);

  // Set IPC child. It should be called at vsync dispatcher thread.
  virtual void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild) MOZ_OVERRIDE;

  virtual void SetVsyncRate(uint32_t aVsyncRate) MOZ_OVERRIDE;

private:
  virtual ~VsyncDispatcherClientImpl();

  void EnableVsyncEvent(bool aEnable);

private:
  static StaticRefPtr<VsyncDispatcherClientImpl> sVsyncDispatcherClient;

  uint32_t mVsyncRate;
  bool mInited;

  bool mVsyncEventNeeded;

  // IPC child
  layers::VsyncEventChild* mVsyncEventChild;

  // Refresh driver
  RefreshDriverRegistry* mRefreshDriver;

  // Vsync event tick timestamp.
  TimeStamp mCurrentTimestamp;
  // Monotonic increased frame number.
  uint64_t mCurrentFrameNumber;
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcherClientImpl_h
