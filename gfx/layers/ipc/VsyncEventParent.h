/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_VSYNC_EVENT_PARENT_H
#define MOZILLA_VSYNC_EVENT_PARENT_H

#include "mozilla/layers/PVsyncEventParent.h"
#include "nsAutoPtr.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class MessageLoop;

namespace mozilla {
namespace layers {

class VsyncEventParent MOZ_FINAL : public PVsyncEventParent
{
  // Top level protocol should be deleted at main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncEventParent);

public:
  static PVsyncEventParent* Create(Transport* aTransport, ProcessId aOtherProcess);

  VsyncEventParent(Transport* aTransport);
  virtual ~VsyncEventParent();

  virtual bool RecvRegisterVsyncEvent() MOZ_OVERRIDE;
  virtual bool RecvUnregisterVsyncEvent() MOZ_OVERRIDE;

  virtual bool RecvGetVsyncRate(uint32_t *aVsyncRate) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  virtual IToplevelProtocol* CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                           ProcessHandle aPeerProcess,
                                           ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

private:
  void DestroyTask();
  void MainThreadDestroyTask();

  Transport* mTransport;

  // Hold a reference to ourself to use message loop.
  // We will release the reference in ActorDestroy().
  nsRefPtr<VsyncEventParent> mVsyncEventParent;
};

} //layers
} //mozilla
#endif  //MOZILLA_VSYNC_EVENT_PARENT_H
