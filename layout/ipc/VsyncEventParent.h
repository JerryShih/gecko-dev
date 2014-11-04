/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ipc_VsyncEventParent_h
#define mozilla_layout_ipc_VsyncEventParent_h

#include "mozilla/layout/PVsyncEventParent.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsISupportsImpl.h"

namespace mozilla {

namespace ipc {
class BackgroundParentImpl;
}

namespace layout {

class VsyncEventParent MOZ_FINAL : public PVsyncEventParent,
                                   public VsyncObserver
{
  friend class mozilla::ipc::BackgroundParentImpl;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncEventParent);

private:
  static PVsyncEventParent* Create();

  VsyncEventParent();
  virtual ~VsyncEventParent();

  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) MOZ_OVERRIDE;

  virtual bool RecvObserveVsync() MOZ_OVERRIDE;
  virtual bool RecvUnobserveVsync() MOZ_OVERRIDE;

  virtual bool RecvGetVsyncRate(uint32_t *aVsyncRate) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  void Destroy();

  void NotifyVsyncInternal(TimeStamp aTimestamp);

  MessageLoop* mMessageLoop;

  // Hold a reference to ourself. It will be released the reference in Destroy().
  nsRefPtr<VsyncEventParent> mVsyncEventParent;
};

} //layout
} //mozilla
#endif  //mozilla_layout_ipc_VsyncEventParent_h
