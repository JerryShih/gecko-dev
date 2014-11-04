/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncEventChild.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layout {

using namespace base;
using namespace mozilla::ipc;

/*static*/ PVsyncEventChild*
VsyncEventChild::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<VsyncEventChild> vsyncEventChild = new VsyncEventChild();
  vsyncEventChild->mVsyncEventChild = vsyncEventChild;

  return vsyncEventChild;
}

void
VsyncEventChild::Destory()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mVsyncEventChild);

  mVsyncEventChild = nullptr;
}

VsyncEventChild::VsyncEventChild()
  : mObservingVsync(false)
{
}

VsyncEventChild::~VsyncEventChild()
{
}

void
VsyncEventChild::SendObserveVsync()
{
  MOZ_ASSERT(NS_IsMainThread());

  mObservingVsync = true;
  PVsyncEventChild::SendObserveVsync();
}
void
VsyncEventChild::SendUnobserveVsync()
{
  MOZ_ASSERT(NS_IsMainThread());

  mObservingVsync = false;
  PVsyncEventChild::SendUnobserveVsync();
}

bool
VsyncEventChild::RecvNotifyVsyncEvent(const TimeStamp& aVsyncTimestamp)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mVsyncObserver);

  // When we call UnobserveVsync(), there might have other RecvNotifyVsyncEvent()
  // messages in thread queue already. Check "mObservingVsync" flag to skip these
  // vsync tick.
  if (mObservingVsync) {
    mVsyncObserver->NotifyVsync(aVsyncTimestamp);
  }

  return true;
}

bool
VsyncEventChild::RecvReplyVsyncRate(const uint32_t& vsyncRate)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mVsyncObserver);

  mVsyncObserver->UpdateVsyncRate(vsyncRate);

  return true;
}

void
VsyncEventChild::SetVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  mVsyncObserver = aVsyncObserver;
}

} // namespace layout
} // namespace mozilla
