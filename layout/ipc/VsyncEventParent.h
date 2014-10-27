/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ipc_VsyncEventParent_h
#define mozilla_layers_ipc_VsyncEventParent_h

#include "mozilla/layout/PVsyncEventParent.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsRefPtr.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class MessageLoop;

namespace base{
class Thread;
}

namespace mozilla {
namespace layout {

// This class helps to handle the life cycle for VsyncEventParent ipc worker
// thread.
class VsyncEventParentWorkerThreadHolder MOZ_FINAL
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncEventParentWorkerThreadHolder);

public:
  VsyncEventParentWorkerThreadHolder();

  base::Thread* GetThread();
  MessageLoop* GetMessageLoop();

private:
  ~VsyncEventParentWorkerThreadHolder();

  void CreateThread();
  void DestroyThread();

  base::Thread* mWorkerThread;
  MessageLoop* mWorkerMessageLoop;
};

class VsyncEventParent MOZ_FINAL : public PVsyncEventParent,
                                   public VsyncObserver
{
public:
  static PVsyncEventParent* Create(Transport* aTransport, ProcessId aOtherProcess);

  VsyncEventParent(Transport* aTransport);

  virtual bool NotifyVsync(TimeStamp aTimestamp) MOZ_OVERRIDE;

  virtual bool RecvRegisterVsyncEvent() MOZ_OVERRIDE;
  virtual bool RecvUnregisterVsyncEvent() MOZ_OVERRIDE;

  virtual bool RecvGetVsyncRate(uint32_t *aVsyncRate) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  virtual IToplevelProtocol* CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                           ProcessHandle aPeerProcess,
                                           ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

private:
  virtual ~VsyncEventParent();

  bool InVsyncEventParentThread();

  MessageLoop* GetMessageLoop();

  void DestroyTask();

  Transport* mTransport;

  nsRefPtr<VsyncEventParentWorkerThreadHolder> mThreadHolder;

  // Hold a reference to ourself to deferred the release to main thread.
  // We will release the reference in ActorDestroy().
  nsRefPtr<VsyncEventParent> mVsyncEventParent;
};

} //layers
} //mozilla
#endif  //mozilla_layers_ipc_VsyncEventParent_h
