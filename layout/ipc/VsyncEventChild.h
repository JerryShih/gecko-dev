/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ipc_VsyncEventChild_h
#define mozilla_layers_ipc_VsyncEventChild_h

#include "mozilla/layout/PVsyncEventChild.h"
#include "nsRefPtr.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class MessageLoop;

namespace mozilla {

class VsyncObserver;

namespace layout {

class VsyncEventChild MOZ_FINAL : public PVsyncEventChild
{
  // Top level protocol should be created and deleted at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncEventChild);

public:
  static PVsyncEventChild* Create(Transport* aTransport,
                                  ProcessId aOtherProcess);

  static VsyncEventChild* GetInstance();

  VsyncEventChild(Transport* aTransport);

  virtual bool RecvNotifyVsyncEvent(const VsyncData& aVsyncData) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  void SetVsyncObserver(VsyncObserver* aVsyncObserver);

private:
  virtual ~VsyncEventChild();

  bool InVsyncEventChildThread();

  void DestroyTask();

  Transport* mTransport;

  VsyncObserver* mVsyncObserver;

  // Hold a reference to ourself to release at ActorDestroy().
  static nsRefPtr<VsyncEventChild> sVsyncEventChild;
};

} // namespace layout
} // namespatce mozilla
#endif  // mozilla_layers_ipc_VsyncEventChild_h
