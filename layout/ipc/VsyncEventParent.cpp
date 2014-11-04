/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncEventParent.h"
#include "mozilla/unused.h"
#include "BackgroundParent.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layout {

using namespace base;
using namespace mozilla::ipc;

/*static*/ PVsyncEventParent*
VsyncEventParent::Create()
{
  AssertIsOnBackgroundThread();

  nsRefPtr<VsyncEventParent> vsyncParent = new VsyncEventParent;
  vsyncParent->mVsyncEventParent = vsyncParent;

  return vsyncParent;
}

VsyncEventParent::VsyncEventParent()
  : mIPCThread(NS_GetCurrentThread())
{
  MOZ_ASSERT(mIPCThread);
  AssertIsOnBackgroundThread();
}

VsyncEventParent::~VsyncEventParent()
{
  AssertIsOnBackgroundThread();
}

void
VsyncEventParent::Destroy()
{
  MOZ_ASSERT(mVsyncEventParent);
  AssertIsOnBackgroundThread();

  mVsyncEventParent = nullptr;
}

bool
VsyncEventParent::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // This function is called by vsync thread, so we need to post to PBackground
  // thread.
  nsRefPtr<nsIRunnable> notifyTask =
      NS_NewRunnableMethodWithArg<TimeStamp>(this,
                                             &VsyncEventParent::NotifyVsyncInternal,
                                             aVsyncTimestamp);
  NS_DispatchToMainThread(notifyTask);

  return true;
}

void
VsyncEventParent::NotifyVsyncInternal(TimeStamp aVsyncTimestamp)
{
  AssertIsOnBackgroundThread();

  // Send ipc to content process.
  unused << SendNotifyVsyncEvent(aVsyncTimestamp);
}

bool
VsyncEventParent::RecvObserveVsync()
{
  AssertIsOnBackgroundThread();

  VsyncDispatcher::GetInstance()->AddRefreshDriverVsyncObserver(this);

  return true;
}

bool
VsyncEventParent::RecvUnobserveVsync()
{
  AssertIsOnBackgroundThread();

  VsyncDispatcher::GetInstance()->RemoveRefreshDriverVsyncObserver(this);

  return true;
}

bool
VsyncEventParent::RecvRequestVsyncRate()
{
  uint32_t vsyncRate = VsyncDispatcher::GetInstance()->GetVsyncRate();
  MOZ_ASSERT(vsyncRate);

  SendReplyVsyncRate(vsyncRate);

  return true;
}

void
VsyncEventParent::ActorDestroy(ActorDestroyReason)
{
  AssertIsOnBackgroundThread();

  VsyncDispatcher::GetInstance()->RemoveRefreshDriverVsyncObserver(this);
}

} // namespace layout
} // namespace mozilla
