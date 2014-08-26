/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcher_h
#define mozilla_widget_shared_VsyncDispatcher_h

#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class MessageLoop;

namespace mozilla {

namespace layers {
class VsyncEventChild;
class VsyncEventParent;
};

class InputDispatchTrigger;
class CompositorTrigger;
class RefreshDriverTrigger;

class VsyncDispatcherClient;
class VsyncDispatcherHost;

// Vsync event observer
class VsyncObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncObserver);

public:
  // VsyncTickTask() will run at each specific observer work thread.
  // We can put our vsync aligned task in this function.
  virtual void VsyncTickTask(int64_t aTimestampUS, uint32_t aFrameNumber) = 0;

protected:
  virtual ~VsyncObserver() { }

};

/*
 * VsyncDispatcher can dispatch vsync event to all registered observer.
 */
class VsyncDispatcher
{
public:
  static VsyncDispatcher* GetInstance();

  virtual void Startup() = 0;
  virtual void Shutdown() = 0;

  // Vsync event rate per second.
  virtual uint32_t GetVsyncRate() = 0;

  virtual InputDispatchTrigger* AsInputDispatchTrigger();
  virtual RefreshDriverTrigger* AsRefreshDriverTrigger();
  virtual CompositorTrigger* AsCompositorTrigger();

  virtual VsyncDispatcherClient* AsVsyncDispatcherClient();
  virtual VsyncDispatcherHost* AsVsyncDispatcherHost();

protected:
  virtual ~VsyncDispatcher() { }
};

// Enable/disable input dispatch when vsync event comes.
class InputDispatchTrigger
{
public:
  virtual void EnableInputDispatcher() = 0;
  virtual void DisableInputDispatcher(bool aSync = false) = 0;

protected:
  virtual ~InputDispatchTrigger() { }
};

// RefreshDriverTrigger will call all registered refresh driver timer
// VsyncTickTask() when vsync event comes.
class RefreshDriverTrigger
{
public:
  // RefreshDriverTrigger is one-shot trigger. We should call RegisterTimer()
  // again if we need next tick.
  virtual void RegisterTimer(VsyncObserver* aTimer) = 0;
  virtual void UnregisterTimer(VsyncObserver* aTimer, bool aSync = false) = 0;

protected:
  virtual ~RefreshDriverTrigger() { }
};

// CompositorTrigger will call registered CompositorParent VsyncTickTask()
// when vsync event comes.
class CompositorTrigger
{
public:
  // CompositorTrigger is one-shot trigger. We should call RegisterCompositor()
  // again if we need next tick.
  virtual void RegisterCompositor(VsyncObserver* aCompositor) = 0;
  virtual void UnregisterCompositor(VsyncObserver* aCompositor, bool aSync = false) = 0;

protected:
  virtual ~CompositorTrigger() { }
};

class VsyncDispatcherClient
{
public:
  // Dispatch vsync to all observer
  virtual void DispatchVsyncEvent(int64_t aTimestampUS, uint32_t aFrameNumber) = 0;

  // Set IPC child. It should be called at vsync dispatcher thread.
  virtual void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild) = 0;

  virtual void SetVsyncRate(uint32_t aVsyncRate) = 0;

protected:
  virtual ~VsyncDispatcherClient() { }
};

class VsyncDispatcherHost
{
public:
  // Notify the VsyncDispatcher that there has one vsync event.
  // VsyncDispatcher will start to dispatch vsync event to all observer.
  virtual void NotifyVsync(int64_t aTimestampUS) = 0;

  // Set IPC parent.
  virtual void RegisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) = 0;
  virtual void UnregisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent) = 0;

  // Get VsyncDispatcher's message loop
  virtual MessageLoop* GetMessageLoop() = 0;

protected:
  virtual ~VsyncDispatcherHost() { }
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcher_h
