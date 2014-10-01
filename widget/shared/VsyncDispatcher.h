/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcher_h
#define mozilla_widget_shared_VsyncDispatcher_h

#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "nsArray.h"

class MessageLoop;

namespace mozilla {

class TimeStamp;

namespace layers {
class VsyncEventChild;
class VsyncEventParent;
};

class VsyncDispatcherClient;
class VsyncDispatcherHost;

// Every vsync event observer should inherit this base class.
// TickVsync() will be called by VsyncEventRegistry when a vsync event comes.
// We should implement this function for our vsync-aligned task.
class VsyncObserver
{
public:
  // The vsync-aligned task. Return true if there has a task ticked.
  virtual bool TickVsync(int64_t aTimeStampNanosecond,
                         TimeStamp aTimestamp,
                         int64_t aTimeStampJS,
                         uint64_t aFrameNumber) = 0;

protected:
  virtual ~VsyncObserver() { }
};

// This class provide the registering interface for vsync observer.
// It will also call observer's TickVsync() when a vsync event comes.
class VsyncEventRegistry
{
public:
  virtual uint32_t GetObserverNum() const = 0;

  // Add/Remove vsync observer.
  // AddObserver() can always trigger or not always triger the TickVsync()
  // per vsync. If aAlwaysTrigger is false, the observer needs another tick,
  // and it should call AddObserver() again.
  // All vsync observers should call sync RemoveObserver() before they
  // call destructor, so we will not tick the observer after destroyed.
  virtual void AddObserver(VsyncObserver* aVsyncObserver,
                           bool aAlwaysTrigger) = 0;
  virtual void RemoveObserver(VsyncObserver* aVsyncObserver,
                              bool aAlwaysTrigger,
                              bool aSync = false) = 0;

protected:
  virtual ~VsyncEventRegistry() {}
};

// VsyncDispatcher is used to dispatch vsync events to the registered observers.
class VsyncDispatcher
{
public:
  static VsyncDispatcher* GetInstance();

  virtual void Startup() = 0;
  virtual void Shutdown() = 0;

  // Vsync event rate per second.
  virtual uint32_t GetVsyncRate() const = 0;

  // Get the VDClient or VDHost.
  // We should only use VDHost at Chrome process and use VDClient at Content.
  virtual VsyncDispatcherClient* AsVsyncDispatcherClient();
  virtual VsyncDispatcherHost* AsVsyncDispatcherHost();

  // Get Registry for refresh driver and compositor.
  virtual VsyncEventRegistry* GetInputDispatcherRegistry();
  virtual VsyncEventRegistry* GetRefreshDriverRegistry();
  virtual VsyncEventRegistry* GetCompositorRegistry();

protected:
  virtual ~VsyncDispatcher() { }
};

// The VDClient for content process
class VsyncDispatcherClient
{
public:
  // Dispatch vsync to all observer
  virtual void DispatchVsyncEvent(int64_t aTimestampNanosecond,
                                  TimeStamp aTimestamp,
                                  int64_t aTimeStampJS,
                                  uint64_t aFrameNumber) = 0;

  // Set vsync event IPC child.
  virtual void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild) = 0;

  // Set the vsync rate getting from VDHost. It will be used by IPC child at
  // initial phase.
  // VDClient will cache this value for Content process VsyncRate query.
  virtual void SetVsyncRate(uint32_t aVsyncRate) = 0;

protected:
  virtual ~VsyncDispatcherClient() { }
};

// The VDHost for chrome process
class VsyncDispatcherHost
{
public:
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
