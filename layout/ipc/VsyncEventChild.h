/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ipc_VsyncEventChild_h
#define mozilla_layout_ipc_VsyncEventChild_h

#include "mozilla/layout/PVsyncEventChild.h"
#include "nsISupportsImpl.h"
#include "nsRefPtr.h"

class MessageLoop;

namespace mozilla {

class VsyncObserver;

namespace ipc {
class BackgroundChildImpl;
}

namespace layout {

class VsyncEventChild MOZ_FINAL : public PVsyncEventChild
{
  friend class mozilla::ipc::BackgroundChildImpl;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncEventChild);

public:
  void SetVsyncObserver(VsyncObserver* aVsyncObserver);

private:
  static PVsyncEventChild* Create();

  VsyncEventChild();
  virtual ~VsyncEventChild();

  virtual bool RecvNotifyVsyncEvent(const TimeStamp& aVsyncTimestamp) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  void Destory();

  VsyncObserver* mVsyncObserver;

  // Hold a reference to ourself. It will be released the reference in Destroy().
  nsRefPtr<VsyncEventChild> mVsyncEventChild;
};

} // namespace layout
} // namespatce mozilla
#endif  // mozilla_layout_ipc_VsyncEventChild_h
