/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/VsyncEventParent.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace layers {

using namespace base;
using namespace mozilla::ipc;

static void
ConnectVsyncEventParent(PVsyncEventParent* aVsyncEventParent,
                        Transport* aTransport,
                        ProcessHandle aOtherProcess)
{
  aVsyncEventParent->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ParentSide);
}

/*static*/ PVsyncEventParent*
VsyncEventParent::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  MOZ_ASSERT(NS_IsMainThread());

  ProcessHandle processHandle;
  if (!OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  nsRefPtr<VsyncEventParent> vsyncParent = new VsyncEventParent(aTransport);
  vsyncParent->mVsyncEventParent = vsyncParent;

  // Use VsyncDispatcher thread for ipc.
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherHost()->
      GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(
                                 &ConnectVsyncEventParent,
                                 vsyncParent,
                                 aTransport,
                                 processHandle));

  return vsyncParent;
}

VsyncEventParent::VsyncEventParent(Transport* aTransport)
  : mTransport(aTransport)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncEventParent::~VsyncEventParent()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
}

void
VsyncEventParent::DestroyTask()
{
  // We should release VsyncEventParent at main thread.
  // NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION will
  // help us to to this.
  mVsyncEventParent = nullptr;
}

bool
VsyncEventParent::RecvRegisterVsyncEvent()
{
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherHost()->RegisterVsyncEventParent(this);

  return true;
}

bool
VsyncEventParent::RecvUnregisterVsyncEvent()
{
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherHost()->UnregisterVsyncEventParent(this);

  return true;
}

bool
VsyncEventParent::RecvGetVsyncRate(uint32_t *aVsyncRate)
{
  *aVsyncRate = VsyncDispatcher::GetInstance()->GetVsyncRate();

  return true;
}

void
VsyncEventParent::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherHost()->UnregisterVsyncEventParent(this);

  // Top level protocol actor should be delete at main thread.
  // We don't post the deletion task to main thread directly here.
  // Instead, we post a pending DeleteTask in VsyncDispatcher Thread.
  // It prevents the problem that ipc system access VsyncEventParent's data
  // when main thread starts to delete the VsyncEventParent.
  VsyncDispatcher::GetInstance()->AsVsyncDispatcherHost()
      ->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
                                   this,
                                   &VsyncEventParent::DestroyTask));
}

IToplevelProtocol*
VsyncEventParent::CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                ProcessHandle aPeerProcess,
                                ProtocolCloneContext* aCtx)
{
  for (unsigned int i = 0; i < aFds.Length(); i++) {
    if (aFds[i].protocolId() == unsigned(GetProtocolId())) {
      Transport* transport = OpenDescriptor(aFds[i].fd(), Transport::MODE_SERVER);
      PVsyncEventParent* vsync = Create(transport, GetProcId(aPeerProcess));

      vsync->CloneManagees(this, aCtx);
      vsync->IToplevelProtocol::SetTransport(transport);

      return vsync;
    }
  }
  return nullptr;
}

} //layers
} //mozilla
