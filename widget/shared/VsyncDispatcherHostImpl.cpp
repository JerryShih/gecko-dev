/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherHostImpl.h"
#include "mozilla/layers/VsyncEventParent.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "VsyncDispatcherHelper.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkVsyncDispatcher.h"
#endif

namespace mozilla {

using namespace layers;

StaticRefPtr<VsyncDispatcherHostImpl> VsyncDispatcherHostImpl::sVsyncDispatcherHost;

/*static*/ VsyncDispatcherHostImpl*
VsyncDispatcherHostImpl::GetInstance()
{
  if (!sVsyncDispatcherHost) {
    // Because we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION,
    // the first call of GetInstance() should at main thread.
#ifdef MOZ_WIDGET_GONK
    sVsyncDispatcherHost = new GonkVsyncDispatcher();
#endif
    //TODO: put other platform's VsyncDispatcher here
  }

  return sVsyncDispatcherHost;
}

VsyncDispatcherHost*
VsyncDispatcherHostImpl::AsVsyncDispatcherHost()
{
  return this;
}

InputDispatchTrigger*
VsyncDispatcherHostImpl::AsInputDispatchTrigger()
{
  return this;
}

RefreshDriverTrigger*
VsyncDispatcherHostImpl::AsRefreshDriverTrigger()
{
  return this;
}

CompositorTrigger*
VsyncDispatcherHostImpl::AsCompositorTrigger()
{
  return this;
}

void
VsyncDispatcherHostImpl::CreateVsyncDispatchThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mVsyncDispatchHostThread, "VDHost thread already existed.");

  mVsyncDispatchHostThread = new base::Thread("Vsync dispatch thread");

  bool startThread = mVsyncDispatchHostThread->Start();
  MOZ_RELEASE_ASSERT(startThread, "Start VDHost thread failed.");

  mVsyncDispatchHostMessageLoop = mVsyncDispatchHostThread->message_loop();
  MOZ_RELEASE_ASSERT(mVsyncDispatchHostMessageLoop, "Get VDHost message loop failed.");
}

void
VsyncDispatcherHostImpl::Startup()
{
  MOZ_ASSERT(NS_IsMainThread(), "VDHost StartUp() should in main thread.");
  MOZ_ASSERT(!mInited, "VDHost is already initialized.");

  mInited = true;

  // We create a internal thread here. VsyncDispatcher can still do dispatch
  // even when the main thread is busy.
  CreateVsyncDispatchThread();

  // We only start up the vsync event at chrome process.
  // Content side doesn't need to do this. Chrome will send the vsync event
  // to content via ipc channel.
  StartupVsyncEvent();
}

void
VsyncDispatcherHostImpl::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "VDHost ShutDown() should in main thread.");
  MOZ_ASSERT(mInited, "VDHost is not initialized.");

  ShutdownVsyncEvent();

  // Wait all pending task in vsync dispatcher thread and delete the
  // vsync dispatcher thread.
  mVsyncDispatchHostMessageLoop = nullptr;
  // Deleting the thread will also wait all tasks in message loop finished.
  delete mVsyncDispatchHostThread;
  mVsyncDispatchHostThread = nullptr;

  mInited = false;
  // Release the VsyncDispatcher singleton.
  sVsyncDispatcherHost = nullptr;
}

VsyncDispatcherHostImpl::VsyncDispatcherHostImpl()
  : mVsyncRate(0)
  , mInited(false)
  , mVsyncDispatchHostThread(nullptr)
  , mVsyncDispatchHostMessageLoop(nullptr)
  , mVsyncEventNeeded(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDispatcherHostImpl::~VsyncDispatcherHostImpl()
{
  MOZ_ASSERT(NS_IsMainThread());

  mVsyncEventParentList.Clear();
}

void
VsyncDispatcherHostImpl::NotifyVsync(int64_t aTimestampUS)
{
  MOZ_ASSERT(mInited, "VDHost is not initialized.");

  GetMessageLoop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                             &VsyncDispatcherHostImpl::NotifyVsyncTask,
                             aTimestampUS));
}

void
VsyncDispatcherHostImpl::NotifyVsyncTask(int64_t aTimestampUS)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  // We propose a monotonic increased frame number here.
  // It helps us to identify the frame count for each vsync update.
  static uint32_t frameNumber = 0;

  DispatchVsyncEvent(aTimestampUS, ++frameNumber);
}

void
VsyncDispatcherHostImpl::DispatchVsyncEvent(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  if (!mVsyncEventNeeded) {
    return;
  }

  // Notify the main thread to handle input event.
  DispatchInputEvent(aTimestampUS, aFrameNumber);

  // Do compose.
  Compose(aTimestampUS, aFrameNumber);

  // Send vsync event to content process
  NotifyContentProcess(aTimestampUS, aFrameNumber);

  // Tick chrome refresh driver.
  TickRefreshDriver(aTimestampUS, aFrameNumber);
}

int
VsyncDispatcherHostImpl::GetVsyncObserverCount()
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  //TODO: If we have more type of vsync observer, we need to count all of them here.

  return mVsyncEventParentList.Length();
}

void
VsyncDispatcherHostImpl::RegisterVsyncEventParent(VsyncEventParent* aVsyncEventParent)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  ObserverListHelper::Add(this, &mVsyncEventParentList, aVsyncEventParent);
}

void
VsyncDispatcherHostImpl::UnregisterVsyncEventParent(VsyncEventParent* aVsyncEventParent)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  ObserverListHelper::Remove(this, &mVsyncEventParentList, aVsyncEventParent);
}

void
VsyncDispatcherHostImpl::DispatchInputEvent(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  //TODO: notify chrome to dispatch input
}

void
VsyncDispatcherHostImpl::Compose(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  //TODO: notify compositor parent to do compose
}

void
VsyncDispatcherHostImpl::TickRefreshDriver(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  //TODO: Tick chrome refresh driver
}

void
VsyncDispatcherHostImpl::NotifyContentProcess(int64_t aTimestampUS, uint32_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  VsyncData vsyncData(aTimestampUS, aFrameNumber);

  // Send ipc to content process.
  for (VsyncEventParentList::size_type i = 0; i < mVsyncEventParentList.Length(); ++i) {
    VsyncEventParent* parent = mVsyncEventParentList[i];
    parent->SendNotifyVsyncEvent(vsyncData);
  }
}

MessageLoop*
VsyncDispatcherHostImpl::GetMessageLoop()
{
  MOZ_ASSERT(mVsyncDispatchHostMessageLoop, "VDHost message is not ready.");

  return mVsyncDispatchHostMessageLoop;
}

void
VsyncDispatcherHostImpl::EnableVsyncNotificationIfhasObserver()
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  // We check the observer number here to enable/disable vsync event
  if (!!GetVsyncObserverCount() !=  mVsyncEventNeeded) {
    mVsyncEventNeeded = !mVsyncEventNeeded;

    EnableVsyncEvent(mVsyncEventNeeded);
  }
}

bool
VsyncDispatcherHostImpl::IsInVsyncDispatcherThread()
{
  return (mVsyncDispatchHostThread->thread_id() == PlatformThread::CurrentId());
}

void
VsyncDispatcherHostImpl::EnableInputDispatcher()
{
  //TODO: enable input dispatcher
}

void
VsyncDispatcherHostImpl::DisableInputDispatcher(bool aSync)
{
  //TODO: disable input dispatcher
  //      With sync flag, we should block here until we actually disable the
  //      InputDispatcher at vsync dispatcher thread.
}

void
VsyncDispatcherHostImpl::RegisterCompositor(VsyncObserver* aCompositor)
{
  //TODO: register a CompositorParent
}

void
VsyncDispatcherHostImpl::UnregisterCompositor(VsyncObserver* aCompositor, bool aSync)
{
  //TODO: unregister a CompositorParent.
  //      With sync flag, we should be blocked here until we actually remove the
  //      CompositorParent at vsync dispatcher thread.
}

void
VsyncDispatcherHostImpl::RegisterTimer(VsyncObserver* aTimer)
{
  //TODO: register a refresh driver timer
}

void
VsyncDispatcherHostImpl::UnregisterTimer(VsyncObserver* aTimer, bool aSync)
{
  //TODO: unregister a refresh driver timer
  //      With sync flag, we should be blocked here until we actually remove the
  //      timer at vsync dispatcher thread.
}

uint32_t
VsyncDispatcherHostImpl::GetVsyncRate() const
{
  MOZ_ASSERT(mVsyncRate);

  return mVsyncRate;
}

} // namespace mozilla
