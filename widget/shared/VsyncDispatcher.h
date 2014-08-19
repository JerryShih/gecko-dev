/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VsyncDispatcher_h
#define mozilla_VsyncDispatcher_h

namespace mozilla {

class CompositorTrigger;
class RefreshDriverTrigger;

// Vsync event observer
class VsyncObserver
{
public:
  virtual void VsyncTick(int64_t aTimestampUS, uint32_t aFrameNumber) = 0;

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

  virtual RefreshDriverTrigger* AsRefreshDriverTrigger()
  {
    return nullptr;
  }

  virtual CompositorTrigger* AsCompositorTrigger()
  {
    return nullptr;
  }

protected:
  virtual ~VsyncDispatcher() { }
};

// RefreshDriverTrigger will call all registered refresh driver timer
// VsyncTick() when vsync event comes.
class RefreshDriverTrigger
{
public:
  virtual void RegisterTimer(VsyncObserver* aTimer) = 0;
  virtual void UnregisterTimer(VsyncObserver* aTimer, bool aSync = false) = 0;

protected:
  virtual ~RefreshDriverTrigger() { }
};

// CompositorTrigger will call registered CompositorParent VsyncTick()
// when vsync event comes.
class CompositorTrigger
{
public:
  virtual void RegisterCompositor(VsyncObserver* aCompositor) = 0;
  virtual void UnregisterCompositor(VsyncObserver* aCompositor, bool aSync = false) = 0;

protected:
  virtual ~CompositorTrigger() { }
};

} // namespace mozilla

#endif // mozilla_VsyncDispatcher_h
