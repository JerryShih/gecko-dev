/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncEventParent.h"

namespace mozilla {
namespace layout {

using namespace base;
using namespace mozilla::ipc;

/*static*/ PVsyncEventParent*
VsyncEventParent::Create()
{
  nsRefPtr<VsyncEventParent> vsyncParent = new VsyncEventParent;
  vsyncParent->mVsyncEventParent = vsyncParent;

  return vsyncParent;
}

VsyncEventParent::VsyncEventParent()
  : mMessageLoop(MessageLoop::current())
{
  MOZ_ASSERT(mMessageLoop);
}

VsyncEventParent::~VsyncEventParent()
{
}

void
VsyncEventParent::Destroy()
{
  mVsyncEventParent = nullptr;
}

bool
VsyncEventParent::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(mMessageLoop);

  // This function is called by vsync dispatcher thread, so we need to post to
  // VsyncEvent ipc thread.
  mMessageLoop->PostTask(FROM_HERE, NewRunnableMethod(this,
                                                      &VsyncEventParent::NotifyVsyncInternal,
                                                      aVsyncTimestamp));

  return true;
}

void
VsyncEventParent::NotifyVsyncInternal(TimeStamp aVsyncTimestamp)
{
  // Send ipc to content process.
  if (!SendNotifyVsyncEvent(aVsyncTimestamp)) {
    NS_WARNING("[Silk] Send NotifyVsyncEvent Error");
  }
}

bool
VsyncEventParent::RecvObserveVsync()
{
  VsyncDispatcher::GetInstance()->AddRefreshDriverVsyncObserver(this);

  return true;
}

bool
VsyncEventParent::RecvUnobserveVsync()
{
  VsyncDispatcher::GetInstance()->RemoveRefreshDriverVsyncObserver(this);

  return true;
}

bool
VsyncEventParent::RecvGetVsyncRate(uint32_t *aVsyncRate)
{
  *aVsyncRate = VsyncDispatcher::GetInstance()->GetVsyncRate();

  return true;
}

void
VsyncEventParent::ActorDestroy(ActorDestroyReason)
{
  VsyncDispatcher::GetInstance()->RemoveRefreshDriverVsyncObserver(this);
}

} // namespace layout
} // namespace mozilla
