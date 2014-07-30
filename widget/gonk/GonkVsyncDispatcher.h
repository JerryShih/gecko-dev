/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GonkVsyncDispatcher_h
#define mozilla_GonkVsyncDispatcher_h

#include "mozilla/VsyncDispatcher.h"

#include "base/ref_counted.h"
#include "nsTArray.h"

class MessageLoop;

namespace mozilla {

namespace layers {
class VsyncEventParent;
class VsyncEventChild;
}

class GonkVsyncDispatcher : public VsyncDispatcher,
                            public base::RefCountedThreadSafe<GonkVsyncDispatcher>
{
  friend class base::RefCountedThreadSafe<GonkVsyncDispatcher>;

public:
  // Start up VsyncDispatcher on internal thread
  static void StartUp();
  // Start up VsyncDispatcher on current thread
  static void StartUpOnCurrentThread();

  // Execute shutdown at VsyncDispatcher's message loop
  static void ShutDown();

  static GonkVsyncDispatcher* GetInstance();

  // Notify VsyncDispatcher that we have a vsync event now
  virtual void NotifyVsync(int64_t aTimestamp) MOZ_OVERRIDE;

  // notify that we have vsync event now.
  void DispatchVsyncEvent(int64_t aTimestamp);
  // Notify that we have vsync event now. Only be call by VsyncEvent protocol.
  void DispatchVsyncEvent(int64_t aTimestamp, uint32_t aFrameNumber);

  void SetVsyncEventChild(layers::VsyncEventChild* aVsyncEventChild);

  // Register ipc VsyncEventParent
  void RegisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent);
  void UnregisterVsyncEventParent(layers::VsyncEventParent* aVsyncEventParent);

  // Check the observer number in VsyncDispatcher to enable/disable vsync event
  // notification.
  void CheckVsyncNotification();

  // Get VsyncDispatcher's message loop
  MessageLoop* GetMessageLoop();

  // Enable/disable vsync dispatch
  void EnableVsyncDispatch(bool aEnable);

private:
  // Singleton pattern. Hide constructor and destructor.
  GonkVsyncDispatcher();
  ~GonkVsyncDispatcher();

  // Sent vsync event to VsyncEventChild
  void NotifyVsyncEventChild(int64_t aTimestamp, uint32_t aFrameNumber);

  // Return total registered object number.
  int GetRegistedObjectCount() const;

private:
  typedef nsTArray<layers::VsyncEventParent*> VsyncEventParentList;
  VsyncEventParentList mVsyncEventParentList;

  layers::VsyncEventChild* mVsyncEventChild;

  bool mEnableVsyncDispatch;
  bool mNeedVsyncEvent;
};

} // namespace mozilla

#endif // mozilla_GonkVsyncDispatcher_h
