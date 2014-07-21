/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_VSYNC_EVENT_PARENT_H
#define MOZILLA_VSYNC_EVENT_PARENT_H

#include "mozilla/layers/PVsyncEventParent.h"

namespace mozilla {
namespace layers {

class VsyncEventParent MOZ_FINAL : public PVsyncEventParent
{
public:
  static PVsyncEventParent* Create(Transport* aTransport, ProcessId aOtherProcess);

  VsyncEventParent(MessageLoop* aMessageLoop, Transport* aTransport);
  ~VsyncEventParent();

  virtual bool RecvEnableVsyncEventNotification() MOZ_OVERRIDE;
  virtual bool RecvDisableVsyncEventNotification() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  virtual IToplevelProtocol* CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                           ProcessHandle aPeerProcess,
                                           ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

  MessageLoop* GetMessageLoop();

private:
  MessageLoop* mMessageLoop;
  Transport* mTransport;
};

} //layers
} //mozilla
#endif  //MOZILLA_VSYNC_EVENT_PARENT_H
