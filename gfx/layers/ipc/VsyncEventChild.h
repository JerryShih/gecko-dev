/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VSYNCEVENTCHILD_H
#define MOZILLA_GFX_VSYNCEVENTCHILD_H

#include "mozilla/layers/PVsyncEventChild.h"

class MessageLoop;

namespace mozilla {
namespace layers {

class VsyncEventChild MOZ_FINAL : public PVsyncEventChild
{
public:
  static PVsyncEventChild* Create(Transport* aTransport,
                                  ProcessId aOtherProcess);

  VsyncEventChild(Transport* aTransport);
  virtual ~VsyncEventChild();

  virtual bool RecvNotifyVsyncEvent(const VsyncData& aVsyncData) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

private:
  Transport* mTransport;

  MessageLoop* mMessageLoop;
};

} //layers
} //mozilla
#endif  //MOZILLA_GFX_VSYNCEVENTCHILD_H
