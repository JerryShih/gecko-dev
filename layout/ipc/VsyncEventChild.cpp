/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncEventChild.h"
#include "mozilla/VsyncDispatcher.h"

namespace mozilla {
namespace layout {

using namespace base;
using namespace mozilla::ipc;

/*static*/ PVsyncEventChild*
VsyncEventChild::Create()
{
  nsRefPtr<VsyncEventChild> vsyncEventChild = new VsyncEventChild();
  vsyncEventChild->mVsyncEventChild = vsyncEventChild;

  return vsyncEventChild;
}

void
VsyncEventChild::Destory()
{
  mVsyncEventChild = nullptr;
}

VsyncEventChild::VsyncEventChild()
  : mVsyncObserver(nullptr)
{
}

VsyncEventChild::~VsyncEventChild()
{
}

bool
VsyncEventChild::RecvNotifyVsyncEvent(const TimeStamp& aVsyncTimestamp)
{
  if (mVsyncObserver) {
    mVsyncObserver->NotifyVsync(aVsyncTimestamp);
  }

  return true;
}

void
VsyncEventChild::ActorDestroy(ActorDestroyReason)
{
  mVsyncObserver = nullptr;
}

void
VsyncEventChild::SetVsyncObserver(VsyncObserver* aVsyncObserver)
{
  mVsyncObserver = aVsyncObserver;
}

} // namespace layout
} // namespace mozilla
