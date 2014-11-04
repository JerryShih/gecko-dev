/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ipc_VsyncEventChild_h
#define mozilla_layout_ipc_VsyncEventChild_h

#include "mozilla/layout/PVsyncEventChild.h"
#include "nsRefPtr.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class MessageLoop;

namespace mozilla {

class VsyncObserver;

namespace ipc {
class BackgroundChildImpl;
}

namespace layout {

// The PVsyncEvent child side implementation.
// It uses main thread for ipc worker, so all ipc call should run at main thread.
class VsyncEventChild MOZ_FINAL : public PVsyncEventChild
{
  friend class mozilla::ipc::BackgroundChildImpl;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncEventChild);

public:
  // Bind a VsyncObserver into VsyncEventChild after ipc channel connected.
  void SetVsyncObserver(VsyncObserver* aVsyncObserver);

  // We overwrite PVsyncEventChild::SendObserveVsync() and SendUnobserveVsync()
  // here. It maintains some statuses for internal usage.
  void SendObserveVsync();
  void SendUnobserveVsync();

private:
  static PVsyncEventChild* Create();
  void Destory();

  VsyncEventChild();
  virtual ~VsyncEventChild();

  virtual bool RecvNotifyVsyncEvent(const TimeStamp& aVsyncTimestamp) MOZ_OVERRIDE;
  virtual bool RecvReplyVsyncRate(const uint32_t& vsyncRate) MOZ_OVERRIDE;

  bool mObservingVsync;

  // The content side vsync refresh driver timer.
  nsRefPtr<VsyncObserver> mVsyncObserver;

  // Hold a reference to itself. It will be released in Destroy().
  nsRefPtr<VsyncEventChild> mVsyncEventChild;
};

} // namespace layout
} // namespatce mozilla
#endif  // mozilla_layout_ipc_VsyncEventChild_h
