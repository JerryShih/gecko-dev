/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcher_h
#define mozilla_widget_shared_VsyncDispatcher_h

#include <stdint.h>

class MessageLoop;

namespace mozilla {

class TimeStamp;

namespace layers {
class VsyncEventChild;
class VsyncEventParent;
};

class VsyncEventRegistry;
class VsyncDispatcher;
class VsyncDispatcherClient;
class VsyncDispatcherHost;

// Every vsync event observer should inherit this base class.
// VsyncTick() will be called by VsyncEventRegistry when a vsync event comes.
// We should implement this function for our vsync-aligned task.
class VsyncObserver
{
public:
  // The vsync-aligned task. Return true if there has a task ticked.
  virtual bool VsyncTick(TimeStamp aTimestamp, uint64_t aFrameNumber) = 0;

protected:
  virtual ~VsyncObserver() { }
};

// This class provide the registering interface for vsync observer.
// It will also call observer's VsyncTick() when a vsync event comes.
class VsyncEventRegistry
{
public:
  // Add/Remove vsync observer.
  // If we call AddObserver() with aAlwaysTrigger being true, the observer be
  // tick for every vsync event. Otherwise, the observer will be tick for only
  // one vsync event. If the observer need another tick, it should call
  // AddObserver() again.
  // All vsync observers should call RemoveObserver() before they call
  // destructor, so we will not tick the observer after observers
  // destroyed.
  virtual void AddObserver(VsyncObserver* aVsyncObserver,
                           bool aAlwaysTrigger) = 0;

  virtual void RemoveObserver(VsyncObserver* aVsyncObserver,
                              bool aAlwaysTrigger) = 0;

  // Dispatch vsync event to all registered observer. Return true if registry
  // need next vsync event.
  virtual bool Dispatch(TimeStamp aTimestamp, uint64_t aFrameNumber) = 0;

protected:
  virtual ~VsyncEventRegistry() { }
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

  // Notify VsyncDispatcher that we have observer in registry and need the vsync
  // tick for next frame.
  virtual void VsyncTickNeeded() = 0;

  // Get the VDClient or VDHost.
  // We should only use VDHost at Chrome process and use VDClient at content.
  virtual VsyncDispatcherClient* AsVsyncDispatcherClient();
  virtual VsyncDispatcherHost* AsVsyncDispatcherHost();

  // Get the registry interface.
  virtual VsyncEventRegistry* GetInputDispatcherRegistry();
  virtual VsyncEventRegistry* GetRefreshDriverRegistry();
  virtual VsyncEventRegistry* GetCompositorRegistry();

protected:
  virtual ~VsyncDispatcher() { }
};

// The VDClient for content process.
class VsyncDispatcherClient
{
public:
  // Dispatch vsync to all observer in content process.
  virtual void NotifyVsync(TimeStamp aTimestamp,
                           uint64_t aFrameNumber) = 0;

  // Set vsync event IPC child.
  virtual void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild) = 0;

  // Set the vsync rate getting from VDHost. It will be used by IPC child at
  // initial phase.
  // VDClient will cache this value for Content process query.
  virtual void SetVsyncRate(uint32_t aVsyncRate) = 0;

protected:
  virtual ~VsyncDispatcherClient() { }
};

// The VDHost for Chrome process.
class VsyncDispatcherHost
{
public:
  // Get registry for vsync event IPC parent.
  virtual VsyncEventRegistry* GetVsyncEventParentRegistry() = 0;

protected:
  virtual ~VsyncDispatcherHost() { }
};

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcher_h
