/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/VsyncDispatcherClient.h"
#include "mozilla/layers/VsyncEventChild.h"
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

  VsyncDispatcherClient::GetInstance()->StartUp();

  VsyncEventChild* vsyncEventChild = new VsyncEventChild(aTransport);

  // Use current thread(main thread) for ipc.
  vsyncEventChild->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ChildSide);

  // Get vsync rate from parent side.
  uint32_t vsyncRate = 0;
  vsyncEventChild->SendGetVsyncRate(&vsyncRate);
  VsyncDispatcherClient::GetInstance()->SetVsyncRate(vsyncRate);

  VsyncDispatcherClient::GetInstance()->SetVsyncEventChild(vsyncEventChild);

  return vsyncEventChild;
}

VsyncEventChild::VsyncEventChild(Transport* aTransport)
  : mTransport(aTransport)
{
  mMessageLoop = MessageLoop::current();
}

VsyncEventChild::~VsyncEventChild()
{
  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
}

bool VsyncEventChild::RecvNotifyVsyncEvent(const VsyncData& aVsyncData)
{
  VsyncDispatcherClient::GetInstance()->DispatchVsyncEvent(aVsyncData.timeStampUS(),
                                                           aVsyncData.frameNumber());

  return true;
}

void
VsyncEventChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  VsyncDispatcherClient::GetInstance()->SetVsyncEventChild(nullptr);
  VsyncDispatcherClient::GetInstance()->ShutDown();

  mMessageLoop->PostTask(FROM_HERE, new DeleteTask<VsyncEventChild>(this));
  mMessageLoop = nullptr;
}

} //layers
} //mozilla
