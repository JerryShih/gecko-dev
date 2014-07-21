/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/VsyncEventChild.h"
#include "base/thread.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

//#define VSYNC_EVENT_CHILD_CREATE_THREAD

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

static VsyncEventChild* sVsyncEventChild = nullptr;

static Thread* sVsyncEventChildThread = nullptr;

static bool
CreateVsyncChildThread()
{
  if (sVsyncEventChildThread) {
    return true;
  }

  sVsyncEventChildThread = new Thread("Vsync child ipc thread");

  if (!sVsyncEventChildThread->Start()) {
    delete sVsyncEventChildThread;
    sVsyncEventChildThread = nullptr;
    return false;
  }

  return true;
}

static void
ConnectVsyncEventChild(VsyncEventChild* aVsyncEventChild,
                       Transport* aTransport,
                       ProcessHandle aOtherProcess)
{
  VSYNC_DEBUG_MESSAGE;

  aVsyncEventChild->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ChildSide);

#ifdef MOZ_NUWA_PROCESS
  if (IsNuwaProcess()) {
    aVsyncEventChild->GetMessageLoop()->PostTask(FROM_HERE,
                                                 NewRunnableFunction(NuwaMarkCurrentThread,
                                                 (void (*)(void *))nullptr,
                                                 (void *)nullptr));
  }
#endif
}

PVsyncEventChild*
VsyncEventChild::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  VSYNC_DEBUG_MESSAGE;

  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");

  ProcessHandle processHandle;
  if (!OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

#ifdef VSYNC_EVENT_CHILD_CREATE_THREAD
  if (!CreateVsyncChildThread()) {
    return nullptr;
  }
  sVsyncEventChild = new VsyncEventChild(sVsyncEventChildThread->message_loop(),
                                         aTransport);

  sVsyncEventChild->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(
                                               &ConnectVsyncEventChild,
                                               sVsyncEventChild,
                                               aTransport,
                                               processHandle));
#else
  sVsyncEventChild = new VsyncEventChild(MessageLoop::current(), aTransport);

  sVsyncEventChild->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ChildSide);
#endif

  return sVsyncEventChild;
}

VsyncEventChild*
VsyncEventChild::GetSingleton()
{
  return sVsyncEventChild;
}

VsyncEventChild::VsyncEventChild(MessageLoop* aMessageLoop, Transport* aTransport)
  : mMessageLoop(aMessageLoop)
  , mTransport(aTransport)
{

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
  VSYNC_DEBUG_MESSAGE;

  return true;
}

void
VsyncEventChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  VSYNC_DEBUG_MESSAGE;

  return;
}

MessageLoop*
VsyncEventChild::GetMessageLoop()
{
  return mMessageLoop;
}

} //layers
} //mozilla
