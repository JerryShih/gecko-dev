/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcher_h
#define mozilla_widget_shared_VsyncDispatcher_h

#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "nsArray.h"

class MessageLoop;

namespace mozilla {

namespace layers {
class VsyncEventChild;
class VsyncEventParent;
};

class VsyncDispatcherClient;
class VsyncDispatcherHost;

// Every vsync event observer should inherit this base class.
class VsyncObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncObserver);

public:
  // This function will be called by VsyncEventRegistry when a vsync event comes.
  // We should implement this function for our vsync-aligned task.
  virtual void TickTask(int64_t aTimestampUS, uint64_t aFrameNumber) = 0;

protected:
  virtual ~VsyncObserver() { }
};

// This class provide the registering interface for vsync observer.
// It will also call observer's TickTask() when a vsync event comes.
class VsyncEventRegistry
{
public:
  uint32_t GetObserverNum(void) const;

  // Register/Unregister vsync observer.
  // The Register() call is one-shot registry. We only call TickTask()
  // once per Register(). If observer need another tick, it should call
  // Register() again.
  // All vsync observers should call sync unregister call before they
  // call destructor, so we will not tick the observer after destroyed.
  virtual void Register(VsyncObserver* aVsyncObserver) = 0;
  virtual void Unregister(VsyncObserver* VsyncObserver, bool aSync = false) = 0;

protected:
  virtual ~VsyncEventRegistry() {}

  typedef nsTArray<VsyncObserver*> ObserverList;
  ObserverList mObserverListList;
};

// VsyncDispatcher can dispatch vsync event to all registered observer.
class VsyncDispatcher
{
public:
  static VsyncDispatcher* GetInstance();

  virtual void Startup() = 0;
  virtual void Shutdown() = 0;

  // Vsync event rate per second.
  virtual uint32_t GetVsyncRate() const = 0;

  // Specialize VD to VDClient or VDHost.
  // We should only use VDHost at Chrome process.
  virtual VsyncDispatcherClient* AsVsyncDispatcherClient();
  virtual VsyncDispatcherHost* AsVsyncDispatcherHost();

  // Get Registry for refresh driver and compositor.
  virtual VsyncEventRegistry* GetRefreshDriverRegistry();
  virtual VsyncEventRegistry* GetCompositorRegistry();

protected:
  virtual ~VsyncDispatcher() { }
};

// The VDClient for content process
class VsyncDispatcherClient : public VsyncDispatcher
{
public:
  // Dispatch vsync to all observer
  virtual void DispatchVsyncEvent(int64_t aTimestampUS, uint64_t aFrameNumber) = 0;

  // Set vsync event IPC child.
  virtual void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild) = 0;

  // Update the vsync rate getting from VDHost.
  virtual void SetVsyncRate(uint32_t aVsyncRate) = 0;

protected:
  virtual ~VsyncDispatcherClient() { }
};

// The VDHost for chrome process
class VsyncDispatcherHost : public VsyncDispatcher
{
public:
  // Enable/disable input dispatch when vsync event comes.
  // We only handle the input at chrome process, so VDClient doesn't have this
  // interface.
  virtual void EnableInputDispatcher() = 0;
  virtual void DisableInputDispatcher(bool aSync = false) = 0;

  // Set vsync event IPC parent.
  virtual void RegisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) = 0;
  virtual void UnregisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) = 0;

  // Get VsyncDispatcher's message loop.
  virtual MessageLoop* GetMessageLoop() = 0;

protected:
  virtual ~VsyncDispatcherHost() { }
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcher_h
