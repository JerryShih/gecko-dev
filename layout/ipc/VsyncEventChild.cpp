/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncEventChild.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace layout {

using namespace base;
using namespace mozilla::ipc;

/*static*/ nsRefPtr<VsyncEventChild> VsyncEventChild::sVsyncEventChild;

/*static*/ PVsyncEventChild*
VsyncEventChild::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  MOZ_ASSERT(NS_IsMainThread());

  ProcessHandle processHandle;
  if (!OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  sVsyncEventChild = new VsyncEventChild(aTransport);

  // Use current thread(main thread) for ipc.
  sVsyncEventChild->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ChildSide);

  return sVsyncEventChild;
}

/*static*/
VsyncEventChild* VsyncEventChild::GetInstance()
{
  MOZ_ASSERT(sVsyncEventChild);

  return sVsyncEventChild;
}

VsyncEventChild::VsyncEventChild(Transport* aTransport)
  : mTransport(aTransport)
  , mVsyncObserver(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncEventChild::~VsyncEventChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
}

void
VsyncEventChild::DestroyTask()
{
  MOZ_ASSERT(NS_IsMainThread());

  sVsyncEventChild = nullptr;
}

bool VsyncEventChild::RecvNotifyVsyncEvent(const VsyncData& aVsyncData)
{
  MOZ_ASSERT(InVsyncEventChildThread());

  if (mVsyncObserver) {
    mVsyncObserver->NotifyVsync(aVsyncData.timestamp());
  }

  return true;
}

void
VsyncEventChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  MOZ_ASSERT(InVsyncEventChildThread());

  MessageLoop::current()->PostTask(FROM_HERE,
                                   NewRunnableMethod(this,
                                   &VsyncEventChild::DestroyTask));
}

bool
VsyncEventChild::InVsyncEventChildThread()
{
  return NS_IsMainThread();
}

void
VsyncEventChild::SetVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  mVsyncObserver = aVsyncObserver;
}

} // namespace layout
} // namespace mozilla
