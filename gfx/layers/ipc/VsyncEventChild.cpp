/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/VsyncEventChild.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace layers {

using namespace base;
using namespace mozilla::ipc;

PVsyncEventChild*
VsyncEventChild::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  MOZ_ASSERT(NS_IsMainThread());

  ProcessHandle processHandle;
  if (!OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  VsyncDispatcher::GetInstance()->Startup();

  nsRefPtr<VsyncEventChild> vsyncEventChild = new VsyncEventChild(aTransport);
  vsyncEventChild->mVsyncEventChild = vsyncEventChild;

  // Use current thread(main thread) for ipc.
  vsyncEventChild->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ChildSide);

  // Get vsync rate from parent side.
  uint32_t vsyncRate = 0;
  vsyncEventChild->SendGetVsyncRate(&vsyncRate);
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherClient()->SetVsyncRate(vsyncRate);

  VsyncDispatcher::GetInstance()->AsVsyncDispatcherClient()->SetVsyncEventChild(vsyncEventChild);

  return vsyncEventChild;
}

VsyncEventChild::VsyncEventChild(Transport* aTransport)
  : mTransport(aTransport)
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
  mVsyncEventChild = nullptr;
}

bool VsyncEventChild::RecvNotifyVsyncEvent(const VsyncData& aVsyncData)
{
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherClient()
      ->DispatchVsyncEvent(aVsyncData.timeStampUS(),
                           aVsyncData.frameNumber());

  return true;
}

void
VsyncEventChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherClient()->SetVsyncEventChild(nullptr);
  VsyncDispatcher::GetInstance()->Shutdown();

  MessageLoop::current()->PostTask(FROM_HERE,
                                   NewRunnableMethod(this,
                                   &VsyncEventChild::DestroyTask));
}

} // namespace layers
} // namespace mozilla
