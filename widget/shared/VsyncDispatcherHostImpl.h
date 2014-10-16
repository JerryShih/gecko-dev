/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcherHostImpl_h
#define mozilla_widget_shared_VsyncDispatcherHostImpl_h

#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/VsyncDispatcherHelper.h"
#include "PlatformVsyncTimer.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "VsyncDispatcher.h"

namespace mozilla {

// The host side vsync dispatcher implementation.
class VsyncDispatcherHostImpl MOZ_FINAL : public VsyncDispatcher
                                        , public VsyncTimerObserver
{
  // We would like to create and delete the VsyncDispatcherHostImpl at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncDispatcherHostImpl);

public:
  static VsyncDispatcherHostImpl* GetInstance();

  VsyncDispatcherHostImpl();

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;

  // This function will be called when the registry need vsync tick.
  virtual void VsyncTickNeeded() MOZ_OVERRIDE;

  virtual VsyncEventRegistry* GetInputDispatcherRegistry() MOZ_OVERRIDE;
  virtual VsyncEventRegistry* GetCompositorRegistry() MOZ_OVERRIDE;

  // This function is called by vsync event generator.
  // It will post a notify task to vsync dispatcher thread.
  virtual void NotifyVsync(TimeStamp aTimestamp) MOZ_OVERRIDE;

private:
  virtual ~VsyncDispatcherHostImpl();

  static StaticRefPtr<VsyncDispatcherHostImpl> sVsyncDispatcherHost;

  bool mInited;
  bool mVsyncEventNeeded;

  // Registries
  VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>* mInputDispatcher;
  VsyncEventRegistryImpl<VsyncRegistryThreadSafePolicy>* mCompositor;

  PlatformVsyncTimer* mTimer;

  TimeStamp mCurrentTimestamp;
  uint64_t mCurrentFrameNumber;
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcherHostImpl_h
