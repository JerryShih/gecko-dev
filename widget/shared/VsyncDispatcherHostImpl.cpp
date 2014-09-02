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
#include "VsyncPlatformTimer.h"

namespace mozilla {

using namespace layers;

StaticRefPtr<VsyncDispatcherHostImpl> VsyncDispatcherHostImpl::sVsyncDispatcherHost;

class VsyncEventRegistryHost : public VsyncEventRegistry
{
  friend class ObserverListHelper;

public:
  VsyncEventRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher);
  virtual ~VsyncEventRegistryHost();

  void Init(VsyncDispatcherHostImpl* aVsyncDispatcher);

  // Dispatch vsync event to all registered observer
  virtual void Dispatch(int64_t aTimestampUS, uint64_t aFrameNumber) = 0;

  virtual void Register(VsyncObserver* aVsyncObserver) MOZ_OVERRIDE;
  virtual void Unregister(VsyncObserver* VsyncObserver, bool aSync) MOZ_OVERRIDE;

protected:
  void EnableVsyncNotificationIfhasObserver();

  bool IsInVsyncDispatcherThread();

  MessageLoop* GetMessageLoop();

  VsyncDispatcherHostImpl* mVsyncDispatcher;
};

// The vsync event registry for RefreshDriver
class RefreshDriverRegistryHost MOZ_FINAL : public VsyncEventRegistryHost
{
public:
  RefreshDriverRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher);
  ~RefreshDriverRegistryHost();

  // Tick all registered refresh driver
  virtual void Dispatch(int64_t aTimestampUS, uint64_t aFrameNumber) MOZ_OVERRIDE;
};

// The vsync event registry for Compositor
class CompositorRegistryHost MOZ_FINAL : public VsyncEventRegistryHost
{
public:
  CompositorRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher);
  ~CompositorRegistryHost();

  // Call compositor parent to do compose
  virtual void Dispatch(int64_t aTimestampUS, uint64_t aFrameNumber) MOZ_OVERRIDE;
};

VsyncEventRegistryHost::VsyncEventRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher)
  : mVsyncDispatcher(aVsyncDispatcher)
{
  MOZ_ASSERT(mVsyncDispatcher);
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncEventRegistryHost::~VsyncEventRegistryHost()
{
  MOZ_ASSERT(NS_IsMainThread());

  mObserverListList.Clear();
}

void
VsyncEventRegistryHost::Register(VsyncObserver* aVsyncObserver)
{
  ObserverListHelper::AsyncAdd(this, &mObserverListList, aVsyncObserver);
}

void
VsyncEventRegistryHost::Unregister(VsyncObserver* VsyncObserver, bool aSync)
{
  if (!aSync) {
    ObserverListHelper::AsyncRemove(this, &mObserverListList, VsyncObserver);
  } else {
    ObserverListHelper::SyncRemove(this,
                                   &mObserverListList,
                                   VsyncObserver);
  }
}

void
VsyncEventRegistryHost::EnableVsyncNotificationIfhasObserver()
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mVsyncDispatcher->EnableVsyncNotificationIfhasObserver();
}

bool
VsyncEventRegistryHost::IsInVsyncDispatcherThread()
{
  return mVsyncDispatcher->IsInVsyncDispatcherThread();
}

MessageLoop*
VsyncEventRegistryHost::GetMessageLoop()
{
  return mVsyncDispatcher->GetMessageLoop();
}

RefreshDriverRegistryHost::RefreshDriverRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher)
  : VsyncEventRegistryHost(aVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
}

RefreshDriverRegistryHost::~RefreshDriverRegistryHost()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
RefreshDriverRegistryHost::Dispatch(int64_t aTimestampUS, uint64_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  //TODO: tick all registered refresh driver
}

CompositorRegistryHost::CompositorRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher)
  : VsyncEventRegistryHost(aVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorRegistryHost::~CompositorRegistryHost()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
CompositorRegistryHost::Dispatch(int64_t aTimestampUS, uint64_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  //TODO: notify compositor parent to do compose
}

/*static*/ VsyncDispatcherHostImpl*
VsyncDispatcherHostImpl::GetInstance()
{
  if (!sVsyncDispatcherHost) {
    // Because we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION,
    // the first call of GetInstance() should at main thread.
    sVsyncDispatcherHost = new VsyncDispatcherHostImpl();
  }

  return sVsyncDispatcherHost;
}

VsyncDispatcherHost*
VsyncDispatcherHostImpl::AsVsyncDispatcherHost()
{
  MOZ_ASSERT(mInited);

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

  // Init all vsync event registry;
  mRefreshDriver = new RefreshDriverRegistryHost(this);
  mCompositor = new CompositorRegistryHost(this);

  // We create a internal thread here. VsyncDispatcher can still do dispatch
  // even when the main thread is busy.
  CreateVsyncDispatchThread();

  // Get platform dependent vsync timer
  mTimer = VsyncPlatformTimerFactory::GetTimer();
  MOZ_ASSERT(mTimer);

  // We only start up the vsync timer at chrome process.
  // Content side doesn't need to do this. Chrome will send the vsync event
  // to content via ipc channel.
  mTimer->SetObserver(this);
  mTimer->Startup();
  mVsyncRate = mTimer->GetVsyncRate();
  MOZ_ASSERT(mVsyncRate);
}

void
VsyncDispatcherHostImpl::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "VDHost ShutDown() should in main thread.");
  MOZ_ASSERT(mInited, "VDHost is not initialized.");

  mTimer->Shutdown();
  delete mTimer;
  mTimer = nullptr;

  // Wait all pending task in vsync dispatcher thread and delete the
  // vsync dispatcher thread.
  mVsyncDispatchHostMessageLoop = nullptr;
  // Deleting the thread will also wait all tasks in message loop finished.
  delete mVsyncDispatchHostThread;
  mVsyncDispatchHostThread = nullptr;

  delete mRefreshDriver;
  mRefreshDriver = nullptr;
  delete mCompositor;
  mCompositor = nullptr;

  mVsyncEventParentList.Clear();

  mInited = false;

  // Release the VsyncDispatcher singleton.
  sVsyncDispatcherHost = nullptr;
}

VsyncDispatcherHostImpl::VsyncDispatcherHostImpl()
  : mVsyncRate(0)
  , mInited(false)
  , mVsyncEventNeeded(false)
  , mVsyncDispatchHostThread(nullptr)
  , mVsyncDispatchHostMessageLoop(nullptr)
  , mRefreshDriver(nullptr)
  , mCompositor(nullptr)
  , mTimer(nullptr)
  , mCurrentTimestampUS(0)
  , mCurrentFrameNumber(0)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDispatcherHostImpl::~VsyncDispatcherHostImpl()
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncEventRegistry*
VsyncDispatcherHostImpl::GetRefreshDriverRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mRefreshDriver);

  return mRefreshDriver;
}

VsyncEventRegistry*
VsyncDispatcherHostImpl::GetCompositorRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mCompositor);

  return mCompositor;
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
VsyncDispatcherHostImpl::NotifyVsync(int64_t aTimestampUS)
{
  MOZ_ASSERT(mInited);

  GetMessageLoop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                             &VsyncDispatcherHostImpl::NotifyVsyncTask,
                             aTimestampUS));
}

void
VsyncDispatcherHostImpl::NotifyVsyncTask(int64_t aTimestampUS)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  MOZ_ASSERT(aTimestampUS > mCurrentTimestampUS);
  mCurrentTimestampUS = aTimestampUS;

  // We propose a monotonic increased frame number here.
  // It helps us to identify the frame count for each vsync update.
  ++mCurrentFrameNumber;

  DispatchVsyncEvent();
}

void
VsyncDispatcherHostImpl::DispatchVsyncEvent()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  if (!mVsyncEventNeeded) {
    return;
  }

  // Notify the main thread to handle input event.
  DispatchInputEvent();

  // Do compose.
  Compose();

  // Send vsync event to content process
  NotifyContentProcess();

  // Tick chrome refresh driver.
  TickRefreshDriver();
}

int
VsyncDispatcherHostImpl::GetVsyncObserverCount()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  return mVsyncEventParentList.Length() + mRefreshDriver->GetObserverNum() +
      mCompositor->GetObserverNum();
}

void
VsyncDispatcherHostImpl::DispatchInputEvent()
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  //TODO: notify chrome to dispatch input
}

void
VsyncDispatcherHostImpl::Compose()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mCompositor->Dispatch(mCurrentTimestampUS, mCurrentFrameNumber);
}

void
VsyncDispatcherHostImpl::TickRefreshDriver()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mRefreshDriver->Dispatch(mCurrentTimestampUS, mCurrentFrameNumber);
}

void
VsyncDispatcherHostImpl::NotifyContentProcess()
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  VsyncData vsyncData(mCurrentTimestampUS, mCurrentFrameNumber);

  // Send ipc to content process.
  for (VsyncEventParentList::size_type i = 0; i < mVsyncEventParentList.Length(); ++i) {
    VsyncEventParent* parent = mVsyncEventParentList[i];
    parent->SendNotifyVsyncEvent(vsyncData);
  }
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

MessageLoop*
VsyncDispatcherHostImpl::GetMessageLoop()
{
  MOZ_ASSERT(mVsyncDispatchHostMessageLoop, "VDHost message is not ready.");

  return mVsyncDispatchHostMessageLoop;
}

void
VsyncDispatcherHostImpl::EnableVsyncNotificationIfhasObserver()
{
  MOZ_ASSERT(mTimer);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  // We check the observer number here to enable/disable vsync event
  if (!!GetVsyncObserverCount() !=  mVsyncEventNeeded) {
    mVsyncEventNeeded = !mVsyncEventNeeded;

    mTimer->Enable(mVsyncEventNeeded);
  }
}

bool
VsyncDispatcherHostImpl::IsInVsyncDispatcherThread()
{
  return (mVsyncDispatchHostThread->thread_id() == PlatformThread::CurrentId());
}

uint32_t
VsyncDispatcherHostImpl::GetVsyncRate() const
{
  MOZ_ASSERT(mVsyncRate);

  return mVsyncRate;
}

} // namespace mozilla
