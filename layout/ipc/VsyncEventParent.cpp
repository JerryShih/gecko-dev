/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncEventParent.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "base/thread.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace layout {

using namespace base;
using namespace mozilla::ipc;

// All VsyncEventParents use the same thread for ipc
static StaticRefPtr<VsyncEventParentWorkerThreadHolder> sVsyncEventParentWorkerThreadHolder;

VsyncEventParentWorkerThreadHolder::VsyncEventParentWorkerThreadHolder()
  : mWorkerThread(nullptr)
  , mWorkerMessageLoop(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());

  CreateThread();
}

VsyncEventParentWorkerThreadHolder::~VsyncEventParentWorkerThreadHolder()
{
  MOZ_ASSERT(NS_IsMainThread());

  DestroyThread();
}

void
VsyncEventParentWorkerThreadHolder::CreateThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mWorkerThread);
  MOZ_ASSERT(!mWorkerMessageLoop);

  mWorkerThread = new base::Thread("VsyncEventParent thread");

  bool startThread = mWorkerThread->Start();
  MOZ_RELEASE_ASSERT(startThread, "Start VsyncEventParent thread failed.");

  mWorkerMessageLoop = mWorkerThread->message_loop();
  MOZ_RELEASE_ASSERT(mWorkerMessageLoop, "Get VsyncEventParent message loop failed.");
}

void
VsyncEventParentWorkerThreadHolder::DestroyThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  mWorkerMessageLoop = nullptr;

  // Deleting the thread will also wait all tasks in message loop finished.
  delete mWorkerThread;
  mWorkerThread = nullptr;
}

base::Thread*
VsyncEventParentWorkerThreadHolder::GetThread()
{
  MOZ_ASSERT(mWorkerThread);

  return mWorkerThread;
}

MessageLoop*
VsyncEventParentWorkerThreadHolder::GetMessageLoop()
{
  MOZ_ASSERT(mWorkerMessageLoop);

  return mWorkerMessageLoop;
}

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
    if (aTransport) {
      XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                       new DeleteTask<Transport>(aTransport));
    }

    return nullptr;
  }

  nsRefPtr<VsyncEventParent> vsyncParent = new VsyncEventParent(aTransport);
  vsyncParent->mVsyncEventParent = vsyncParent;

  vsyncParent->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(
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

  // Create the ipc worker thread.
  if (!sVsyncEventParentWorkerThreadHolder) {
    sVsyncEventParentWorkerThreadHolder = new VsyncEventParentWorkerThreadHolder;

    // The reference count for VsyncEventParentWorkerThreadHolder is 1 now.
    // Use ClearOnShutdown() to decrease this 1 reference count for
    // sVsyncEventParentWorkerThreadHolder.
    // ClearOnShutdown() will be called at XPCOM shutdown.
    ClearOnShutdown(&sVsyncEventParentWorkerThreadHolder);
  }

  // Each content process will have a pair of VsyncEventParent and
  // VsyncEventChild.
  // All VsyncEventParent share the same sVsyncEventParentWorkerThreadHolder.
  mThreadHolder = sVsyncEventParentWorkerThreadHolder;
}

VsyncEventParent::~VsyncEventParent()
{
  MOZ_ASSERT(NS_IsMainThread());

  mThreadHolder = nullptr;

  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }

  if (OtherProcess()) {
      CloseProcessHandle(OtherProcess());
  }
}

MessageLoop*
VsyncEventParent::GetMessageLoop()
{
  MOZ_ASSERT(mThreadHolder.get());

  return mThreadHolder->GetMessageLoop();
}

void
VsyncEventParent::DestroyTask()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We should release VsyncEventParent at main thread.
  // NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION will
  // help us to to this.
  mVsyncEventParent = nullptr;
}

bool
VsyncEventParent::NotifyVsync(TimeStamp aTimestamp)
{
  // Send ipc to content process.
  if (!SendNotifyVsyncEvent(VsyncData(aTimestamp))) {
    NS_WARNING("[Silk] Send NotifyVsyncEvent Error");

    return false;
  }

  return true;
}
bool
VsyncEventParent::RecvRegisterVsyncEvent()
{
  MOZ_ASSERT(InVsyncEventParentThread());

  VsyncDispatcher::GetInstance()->AddRefreshDriverVsyncObserver(this);

  return true;
}

bool
VsyncEventParent::RecvUnregisterVsyncEvent()
{
  MOZ_ASSERT(InVsyncEventParentThread());

  VsyncDispatcher::GetInstance()->RemoveRefreshDriverVsyncObserver(this);

  return true;
}

bool
VsyncEventParent::RecvGetVsyncRate(uint32_t *aVsyncRate)
{
  MOZ_ASSERT(InVsyncEventParentThread());

  *aVsyncRate = VsyncDispatcher::GetInstance()->GetVsyncRate();

  return true;
}

void
VsyncEventParent::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  MOZ_ASSERT(InVsyncEventParentThread());

  VsyncDispatcher::GetInstance()->RemoveRefreshDriverVsyncObserver(this);

  // Top level protocol actor should be delete at main thread.
  // We don't post the deletion task to main thread directly here.
  // Instead, we post a pending DeleteTask to VsyncEventParent worker thread.
  // It prevents the problem that ipc system access VsyncEventParent's data
  // when main thread starts to delete the VsyncEventParent.
  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
                             this,
                             &VsyncEventParent::DestroyTask));
}

IToplevelProtocol*
VsyncEventParent::CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                ProcessHandle aPeerProcess,
                                ProtocolCloneContext* aCtx)
{
  MOZ_ASSERT(NS_IsMainThread());

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

bool
VsyncEventParent::InVsyncEventParentThread()
{
  MOZ_ASSERT(mThreadHolder.get());

  return mThreadHolder->GetThread()->thread_id() == PlatformThread::CurrentId();
}

} // namespace layout
} // namespace mozilla
