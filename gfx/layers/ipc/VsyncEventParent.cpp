/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/VsyncEventParent.h"
#include "mozilla/ipc/Transport.h"
#include "base/thread.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkVsyncDispatcher.h"
#endif

//#define PRINT_VSYNC_DEBUG

#ifdef PRINT_VSYNC_DEBUG
#define VSYNC_DEBUG_MESSAGE printf_stderr("bignose tid:%d %s",gettid(),__PRETTY_FUNCTION__)
#else
#define VSYNC_DEBUG_MESSAGE
#endif

using namespace base;
using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

static Thread* sVsyncEventParentThread = nullptr;

static bool
CreateVsyncParentThread()
{
  if (sVsyncEventParentThread) {
    return true;
  }

  sVsyncEventParentThread = new Thread("Vsync parent ipc thread");

  if (!sVsyncEventParentThread->Start()) {
    delete sVsyncEventParentThread;
    sVsyncEventParentThread = nullptr;
    return false;
  }

  return true;
}

static void
ConnectVsyncEventParent(PVsyncEventParent* aVsyncEventParent,
                        Transport* aTransport,
                        ProcessHandle aOtherProcess)
{
  VSYNC_DEBUG_MESSAGE;

  aVsyncEventParent->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ParentSide);
}

/*static*/ PVsyncEventParent*
VsyncEventParent::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  VSYNC_DEBUG_MESSAGE;

  ProcessHandle processHandle;
  if (!OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

#ifdef MOZ_WIDGET_GONK
  GonkVsyncDispatcher::StartUp();

  VsyncEventParent* vsync = new VsyncEventParent(GonkVsyncDispatcher::GetInstance()->GetMessageLoop(),
                                                 aTransport);
#else
  if (!CreateVsyncParentThread()) {
    return nullptr;
  }

  VsyncEventParent* vsync = new VsyncEventParent(sVsyncEventParentThread->message_loop(),
                                                 aTransport);
#endif

  vsync->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(
                                    &ConnectVsyncEventParent,
                                    vsync,
                                    aTransport,
                                    processHandle));

  return vsync;
}

VsyncEventParent::VsyncEventParent(MessageLoop* aMessageLoop, Transport* aTransport)
  : mMessageLoop(aMessageLoop)
  , mTransport(aTransport)
{

}

VsyncEventParent::~VsyncEventParent()
{
  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
}

bool
VsyncEventParent::RecvEnableVsyncEventNotification()
{
  VSYNC_DEBUG_MESSAGE;
#ifdef MOZ_WIDGET_GONK
  GonkVsyncDispatcher::GetInstance()->RegisterVsyncEventParent(this);
#endif

  return true;
}

bool
VsyncEventParent::RecvDisableVsyncEventNotification()
{
  VSYNC_DEBUG_MESSAGE;
#ifdef MOZ_WIDGET_GONK
  GonkVsyncDispatcher::GetInstance()->UnregisterVsyncEventParent(this);
#endif

  return true;
}

void
VsyncEventParent::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  VSYNC_DEBUG_MESSAGE;
#ifdef MOZ_WIDGET_GONK
  GonkVsyncDispatcher::GetInstance()->UnregisterVsyncEventParent(this);
#endif

  return;
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

MessageLoop*
VsyncEventParent::GetMessageLoop()
{
  return mMessageLoop;
}

} //layers
} //mozilla
