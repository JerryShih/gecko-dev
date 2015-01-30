/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncChild.h"

#include "mozilla/VsyncDispatcher.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layout {

VsyncChild::VsyncChild()
  : mObservingVsync(false)
  , mDidShutdown(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncChild::~VsyncChild()
{
  MOZ_ASSERT(NS_IsMainThread());
  printf_stderr("VsyncChild::Destructor\n");
}

bool
VsyncChild::SendObserve()
{
  MOZ_ASSERT(NS_IsMainThread());
  //if (!mObservingVsync && !mDidShutdown) {
  if (!mObservingVsync) {
    printf_stderr("VsyncChild Observe vsync\n");
    PVsyncChild::SendObserve();
    mObservingVsync = true;
  }
  return true;
}

bool
VsyncChild::SendUnobserve()
{
  MOZ_ASSERT(NS_IsMainThread());
  //if (mObservingVsync && !mDidShutdown) {
  if (!mObservingVsync) {
    printf_stderr("VsyncChild unobserve vsync\n");
    PVsyncChild::SendUnobserve();
    mObservingVsync = false;
  }
  return true;
}

void
VsyncChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  mObserver = nullptr;
  //mObservingVsync = false;
  mDidShutdown = true;
  printf_stderr("vsync child actor destroy\n");
}

bool
VsyncChild::RecvNotify(const TimeStamp& aVsyncTimestamp)
{
  MOZ_ASSERT(NS_IsMainThread());
  //if (mObservingVsync && mObserver && !mDidShutdown) {
  if (mObservingVsync && mObserver) {
    printf_stderr("VsyncChild::RecvNotify\n");
    mObservingVsync = false;
    mObserver->NotifyVsync(aVsyncTimestamp);
  }
  return true;
}

void
VsyncChild::SetVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  mObserver = aVsyncObserver;
}

} // namespace layout
} // namespace mozilla
