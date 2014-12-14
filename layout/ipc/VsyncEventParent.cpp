/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncEventParent.h"
#include "mozilla/unused.h"
#include "BackgroundParent.h"
#include "nsBaseWidget.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "mozilla/VsyncDispatcher.h"

namespace mozilla {
namespace layout {

using namespace base;
using namespace mozilla::ipc;

/*static*/ PVsyncEventParent*
VsyncEventParent::Create(const dom::TabId& aTabID)
{
  AssertIsOnBackgroundThread();

  nsRefPtr<VsyncEventParent> vsyncParent = new VsyncEventParent(aTabID);
  vsyncParent->mVsyncEventParent = vsyncParent;

  return vsyncParent;
}

VsyncEventParent::VsyncEventParent(const dom::TabId& aTabID)
  : mIPCThread(NS_GetCurrentThread())
{
  MOZ_ASSERT(mIPCThread);
  AssertIsOnBackgroundThread();

  nsIWidget* widget = nsBaseWidget::GetWidget(aTabID);
  mChromeVsyncDispatcher = static_cast<ChromeVsyncDispatcher*>(widget->GetVsyncDispatcherBase());
  MOZ_ASSERT(mChromeVsyncDispatcher);

  printf_stderr("bignose create VsyncEventParent, id:%d, dispatcher:%p\n",(int32_t)aTabID, mChromeVsyncDispatcher.get());
}

VsyncEventParent::~VsyncEventParent()
{
  AssertIsOnBackgroundThread();
}

void
VsyncEventParent::Destroy()
{
  MOZ_ASSERT(mVsyncEventParent);
  AssertIsOnBackgroundThread();

  mVsyncEventParent = nullptr;
}

void
VsyncEventParent::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  //printf_stderr("bignose VsyncEventParent::NotifyVsync\n");
  // This function is called by vsync thread, so we need to post to PBackground
  // thread.
  nsRefPtr<nsIRunnable> notifyTask =
      NS_NewRunnableMethodWithArg<TimeStamp>(this,
                                             &VsyncEventParent::NotifyVsyncInternal,
                                             aVsyncTimestamp);
  mIPCThread->Dispatch(notifyTask, nsIEventTarget::DISPATCH_NORMAL);
}

void
VsyncEventParent::NotifyVsyncInternal(TimeStamp aVsyncTimestamp)
{
  printf_stderr("bignose VsyncEventParent::NotifyVsync\n");
  AssertIsOnBackgroundThread();

  // Send ipc to content process.
  unused << SendNotifyVsyncEvent(aVsyncTimestamp);
}

bool
VsyncEventParent::RecvObserveVsync()
{
  AssertIsOnBackgroundThread();

  mChromeVsyncDispatcher->AddVsyncEventParent(this);

  return true;
}

bool
VsyncEventParent::RecvUnobserveVsync()
{
  AssertIsOnBackgroundThread();

  mChromeVsyncDispatcher->RemoveVsyncEventParent(this);

  return true;
}

void
VsyncEventParent::ActorDestroy(ActorDestroyReason)
{
  AssertIsOnBackgroundThread();

  mChromeVsyncDispatcher->RemoveVsyncEventParent(this);
  mChromeVsyncDispatcher = nullptr;
}

} // namespace layout
} // namespace mozilla
