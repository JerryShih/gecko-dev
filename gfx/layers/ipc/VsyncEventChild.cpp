/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/VsyncEventChild.h"
#include "nsXULAppAPI.h"

//#define PRINT_VSYNC_DEBUG
#ifdef PRINT_VSYNC_DEBUG
#define VSYNC_DEBUG_MESSAGE printf_stderr("bignose tid:%d %s",gettid(),__PRETTY_FUNCTION__)
#else
#define VSYNC_DEBUG_MESSAGE
#endif

namespace mozilla {
namespace layers {

using namespace base;
using namespace mozilla::ipc;

PVsyncEventChild*
VsyncEventChild::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  VSYNC_DEBUG_MESSAGE;

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  //MOZ_ASSERT(NS_IsMainThread());

  ProcessHandle processHandle;
  if (!OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  VsyncEventChild* vsyncEventChild = nullptr;

#ifdef MOZ_WIDGET_GONK
  // Create VsyncEventChild here
#endif

  return vsyncEventChild;
}

VsyncEventChild::VsyncEventChild(MessageLoop* aMessageLoop, Transport* aTransport)
  : mMessageLoop(aMessageLoop)
  , mTransport(aTransport)
{
  VSYNC_DEBUG_MESSAGE;
}

VsyncEventChild::~VsyncEventChild()
{
  VSYNC_DEBUG_MESSAGE;

  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
}

bool VsyncEventChild::RecvNotifyVsyncEvent(const VsyncData& aVsyncData)
{
  VSYNC_DEBUG_MESSAGE;

  return true;
}

void
VsyncEventChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  VSYNC_DEBUG_MESSAGE;

  GetMessageLoop()->PostTask(FROM_HERE,
                             new DeleteTask<VsyncEventChild>(this));

  return;
}

MessageLoop*
VsyncEventChild::GetMessageLoop()
{
  return mMessageLoop;
}

} //layers
} //mozilla
