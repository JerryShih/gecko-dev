/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ipc_VsyncEventParent_h
#define mozilla_layout_ipc_VsyncEventParent_h

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layout/PVsyncEventParent.h"
#include "nsISupportsImpl.h"
#include "nsCOMPtr.h"

class nsIThread;

namespace mozilla {

class ChromeVsyncDispatcher;

namespace ipc {
class BackgroundParentImpl;
}

namespace layout {

// The PVsyncEvent parent side implementation.
// It uses PBackground thread for ipc worker, so all ipc call should run at
// PBackground thread.
class VsyncEventParent MOZ_FINAL : public PVsyncEventParent
{
  friend class mozilla::ipc::BackgroundParentImpl;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncEventParent);

  void NotifyVsync(TimeStamp aVsyncTimestamp);

private:
  static PVsyncEventParent* Create(const dom::TabId& aTabID);

  VsyncEventParent(const dom::TabId& aTabID);
  virtual ~VsyncEventParent();

  virtual bool RecvObserveVsync() MOZ_OVERRIDE;
  virtual bool RecvUnobserveVsync() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  void Destroy();

  void NotifyVsyncInternal(TimeStamp aTimestamp);

  nsCOMPtr<nsIThread> mIPCThread;

  nsRefPtr<ChromeVsyncDispatcher> mChromeVsyncDispatcher;

  // Hold a reference to ourself. It will be released the reference in Destroy().
  nsRefPtr<VsyncEventParent> mVsyncEventParent;
};

} //layout
} //mozilla
#endif  //mozilla_layout_ipc_VsyncEventParent_h
