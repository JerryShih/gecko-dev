/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

// Debug
#include "VsyncDispatcherTrace.h"
#include "cutils/properties.h"

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
  virtual void Dispatch(int64_t aTimestampNanosecond,
                        TimeStamp aTimestamp,
                        int64_t aTimestampJS,
                        uint64_t aFrameNumber) = 0;

  virtual void Register(VsyncObserver* aVsyncObserver) MOZ_OVERRIDE;
  virtual void Unregister(VsyncObserver* aVsyncObserver, bool aSync) MOZ_OVERRIDE;

  virtual uint32_t GetObserverNum() const MOZ_OVERRIDE;

protected:
  void EnableVsyncNotificationIfhasObserver();

  bool IsInVsyncDispatcherThread() const;

  MessageLoop* GetMessageLoop();

protected:
  VsyncDispatcherHostImpl* mVsyncDispatcher;

  typedef nsTArray<VsyncObserver*> ObserverList;
  ObserverList mObserverListList;
};

// The vsync event registry for InputDispatcher
class InputDispatcherRegistryHost MOZ_FINAL : public VsyncEventRegistry
{
public:
  InputDispatcherRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher);
  ~InputDispatcherRegistryHost();

  // Tick registered InputDispatcher. Return true if input dispatcher dispatches
  // one task.
  bool Dispatch(int64_t aTimestampNanosecond,
                TimeStamp aTimestamp,
                int64_t aTimestampJS,
                uint64_t aFrameNumber);

  virtual void Register(VsyncObserver* aVsyncObserver) MOZ_OVERRIDE;
  virtual void Unregister(VsyncObserver* aVsyncObserver, bool aSync) MOZ_OVERRIDE;

  virtual uint32_t GetObserverNum() const MOZ_OVERRIDE;

private:
  static void SetObserver(VsyncObserver** aLVsyncObserver,
                          VsyncObserver* aRVsyncObserver,
                          Monitor* aMonitor = nullptr,
                          bool* aDone = nullptr);

private:
  VsyncDispatcherHostImpl* mVsyncDispatcher;
  VsyncObserver* mObserver;
};

// The vsync event registry for RefreshDriver
class RefreshDriverRegistryHost MOZ_FINAL : public VsyncEventRegistryHost
{
public:
  RefreshDriverRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher);
  ~RefreshDriverRegistryHost();

  // Tick all registered refresh driver
  virtual void Dispatch(int64_t aTimestampNanosecond,
                        TimeStamp aTimestamp,
                        int64_t aTimestampJS,
                        uint64_t aFrameNumber) MOZ_OVERRIDE;
};

// The vsync event registry for Compositor
class CompositorRegistryHost MOZ_FINAL : public VsyncEventRegistryHost
{
public:
  CompositorRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher);
  ~CompositorRegistryHost();

  // Call compositor parent to do compose
  virtual void Dispatch(int64_t aTimestampNanosecond,
                        TimeStamp aTimestamp,
                        int64_t aTimestampJS,
                        uint64_t aFrameNumber) MOZ_OVERRIDE;
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
VsyncEventRegistryHost::Unregister(VsyncObserver* aVsyncObserver, bool aSync)
{
  if (!aSync) {
    ObserverListHelper::AsyncRemove(this, &mObserverListList, aVsyncObserver);
  } else {
    ObserverListHelper::SyncRemove(this,
                                   &mObserverListList,
                                   aVsyncObserver);
  }
}

uint32_t
VsyncEventRegistryHost::GetObserverNum() const
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  return mObserverListList.Length();
}

void
VsyncEventRegistryHost::EnableVsyncNotificationIfhasObserver()
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mVsyncDispatcher->EnableVsyncNotificationIfhasObserver();
}

bool
VsyncEventRegistryHost::IsInVsyncDispatcherThread() const
{
  return mVsyncDispatcher->IsInVsyncDispatcherThread();
}

MessageLoop*
VsyncEventRegistryHost::GetMessageLoop()
{
  return mVsyncDispatcher->GetMessageLoop();
}

InputDispatcherRegistryHost::InputDispatcherRegistryHost(VsyncDispatcherHostImpl* aVsyncDispatcher)
  : mVsyncDispatcher(aVsyncDispatcher)
  , mObserver(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mVsyncDispatcher);
}

InputDispatcherRegistryHost::~InputDispatcherRegistryHost()
{
  MOZ_ASSERT(NS_IsMainThread());
  Unregister(mObserver, true);
}

bool
InputDispatcherRegistryHost::Dispatch(int64_t aTimestampNanosecond,
                                      TimeStamp aTimestamp,
                                      int64_t aTimestampJS,
                                      uint64_t aFrameNumber)
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  bool isTicked = false;
  if (mObserver) {
    char propValue[PROPERTY_VALUE_MAX];
    property_get("silk.i.lat", propValue, "0");
    if (atoi(propValue) != 0) {
      // Begin Latency
      VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN_PRINTF((int32_t)aFrameNumber, "Input_Latency (%u)", (uint32_t)aFrameNumber);
    }

    isTicked = mObserver->TickVsync(aTimestampNanosecond,
                                    aTimestamp,
                                    aTimestampJS,
                                    aFrameNumber);
    mObserver = nullptr;
  }
  return isTicked;
}

void
InputDispatcherRegistryHost::Register(VsyncObserver* aVsyncObserver)
{
  if (mVsyncDispatcher->IsInVsyncDispatcherThread()) {
    SetObserver(&mObserver, aVsyncObserver);
    return;
  }

  MessageLoop* loop = mVsyncDispatcher->GetMessageLoop();
  loop->PostTask(FROM_HERE,
                 NewRunnableFunction(&InputDispatcherRegistryHost::SetObserver,
                                     &mObserver,
                                     aVsyncObserver,
                                     nullptr,
                                     nullptr));
}

// Don't care aVsyncObserver in this function.
void
InputDispatcherRegistryHost::Unregister(VsyncObserver* aVsyncObserver, bool aSync)
{
  if (mVsyncDispatcher->IsInVsyncDispatcherThread()) {
    // In Vsync Dispatcher Thread, we don't care aSync
    SetObserver(&mObserver, nullptr);
    return;
  }

  if (!aSync) {
    MessageLoop* loop = mVsyncDispatcher->GetMessageLoop();
    loop->PostTask(FROM_HERE,
                   NewRunnableFunction(&InputDispatcherRegistryHost::SetObserver,
                                       &mObserver,
                                       nullptr,
                                       nullptr,
                                       nullptr));
  } else {
    Monitor monitor("InputDispatcherRegistryHost Unregister");
    MonitorAutoLock lock(monitor);
    bool done = false;

    MessageLoop* loop = mVsyncDispatcher->GetMessageLoop();
    loop->PostTask(FROM_HERE,
                   NewRunnableFunction(&InputDispatcherRegistryHost::SetObserver,
                                       &mObserver,
                                       nullptr,
                                       &monitor,
                                       &done));

    while (!done) {
      lock.Wait(PR_MillisecondsToInterval(32));
      printf_stderr("Wait InputDispatcherRegistryHost SetObserver timeout");
    }
  }
}

uint32_t
InputDispatcherRegistryHost::GetObserverNum() const
{
  MOZ_ASSERT(mVsyncDispatcher->IsInVsyncDispatcherThread());

  return mObserver ? 1 : 0;
}

// Assumption: InputDispatcherRegistryHost exists if this function is called
/*static*/ void
InputDispatcherRegistryHost::SetObserver(VsyncObserver** aLVsyncObserver,
                                         VsyncObserver* aRVsyncObserver,
                                         Monitor* aMonitor,
                                         bool* aDone)
{
  MOZ_ASSERT(VsyncDispatcherHostImpl::GetInstance());
  MOZ_ASSERT(VsyncDispatcherHostImpl::GetInstance()->IsInVsyncDispatcherThread());
  // If we already have an input dispatcher, we should not receive another one.
  MOZ_ASSERT_IF(*aLVsyncObserver && aRVsyncObserver,
                *aLVsyncObserver == aRVsyncObserver);

  // If we get a Monitor, we use the sync mode. Notify the monitor after
  // the assignment done.
  if (aMonitor) {
    MonitorAutoLock lock(*aMonitor);

    *aLVsyncObserver = aRVsyncObserver;
    *aDone = true;

    lock.Notify();
  } else {
    *aLVsyncObserver = aRVsyncObserver;
  }

  VsyncDispatcherHostImpl::GetInstance()->EnableVsyncNotificationIfhasObserver();
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
RefreshDriverRegistryHost::Dispatch(int64_t aTimestampNanosecond,
                                    TimeStamp aTimestamp,
                                    int64_t aTimestampJS,
                                    uint64_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  // Tick all content registered refresh drivers.
  //
  // Unlike RefreshDriverRegistryClient::Dispatch(), chrome's refresh driver
  // doesn't run at VsyncDispatcherThread. We always post a task to add/remove
  // the the observer list, so we don't need a list copy here.
  for (ObserverList::size_type i = 0; i < mObserverListList.Length(); i++) {
    char propValue[PROPERTY_VALUE_MAX];
    property_get("silk.r.lat", propValue, "0");
    if (atoi(propValue) != 0) {
      // Begin Latency
      VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN_PRINTF((int32_t)aFrameNumber, "RD_Latency (%u)", (uint32_t)aFrameNumber);
      mObserverListList[i]->TickVsync(aTimestampNanosecond, aTimestamp, aTimestampJS, aFrameNumber);
    } else {
      mObserverListList[i]->TickVsync(aTimestampNanosecond, aTimestamp, aTimestampJS, aFrameNumber);
    }
  }

  mObserverListList.Clear();
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
CompositorRegistryHost::Dispatch(int64_t aTimestampNanosecond,
                                 TimeStamp aTimestamp,
                                 int64_t aTimestampJS,
                                 uint64_t aFrameNumber)
{
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  for (ObserverList::size_type i = 0; i < mObserverListList.Length(); ++i) {
    char propValue[PROPERTY_VALUE_MAX];
    property_get("silk.c.lat", propValue, "0");
    if (atoi(propValue) != 0) {
      // Begin Latency
      VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN_PRINTF((int32_t)aFrameNumber, "Comp_Latency (%u)", (uint32_t)aFrameNumber);
      mObserverListList[i]->TickVsync(aTimestampNanosecond, aTimestamp, aTimestampJS, aFrameNumber);
    } else {
      mObserverListList[i]->TickVsync(aTimestampNanosecond, aTimestamp, aTimestampJS, aFrameNumber);
    }
  }

  mObserverListList.Clear();
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
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
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
  mInputDispatcher = new InputDispatcherRegistryHost(this);
  mRefreshDriver = new RefreshDriverRegistryHost(this);
  mCompositor = new CompositorRegistryHost(this);

  // We create a internal thread here. VsyncDispatcher can still do dispatch
  // even when the main thread is busy.
  CreateVsyncDispatchThread();

  // Get platform dependent vsync timer.
  // We only use the vsync timer at chrome process.
  // Content side doesn't need to do this. Chrome will send the vsync event
  // to content via ipc channel.
  mTimer = PlatformVsyncTimerFactory::Create(this);
  MOZ_ASSERT(mTimer);
  mVsyncRate = mTimer->GetVsyncRate();
  MOZ_ASSERT(mVsyncRate);
  // Start the timer.
  mTimer->Enable(true);
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
  delete mInputDispatcher;
  mInputDispatcher = nullptr;

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
  , mInputDispatcher(nullptr)
  , mRefreshDriver(nullptr)
  , mCompositor(nullptr)
  , mTimer(nullptr)
  , mCurrentTimestampNanosecond(0)
  , mCurrentTimestampJS(0)
  , mCurrentFrameNumber(0)
  , mVsyncFrameNumber(0)
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDispatcherHostImpl::~VsyncDispatcherHostImpl()
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncEventRegistry*
VsyncDispatcherHostImpl::GetInputDispatcherRegistry()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mInputDispatcher);

  return mInputDispatcher;
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
VsyncDispatcherHostImpl::NotifyVsync(int64_t aTimestampNanosecond,
                                     TimeStamp aTimestamp,
                                     int64_t aTimestampJS)
{
  MOZ_ASSERT(mInited);

  // We propose a monotonic increased frame number here.
  // It helps us to identify the frame count for each vsync update.
  ++mVsyncFrameNumber;
  
  // Begin id == time 32bit
  char propValue[PROPERTY_VALUE_MAX];
  property_get("silk.hw2vsync", propValue, "0");
  if (atoi(propValue) != 0) {
    VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN_PRINTF((int32_t)mVsyncFrameNumber, "HwToVsync (%u)", (uint32_t)mVsyncFrameNumber);
  }

  GetMessageLoop()->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                             &VsyncDispatcherHostImpl::NotifyVsyncTask,
                             aTimestampNanosecond,
                             aTimestamp,
                             aTimestampJS,
                             mVsyncFrameNumber));
}

void
VsyncDispatcherHostImpl::NotifyVsyncTask(int64_t aTimestampNanosecond,
                                         TimeStamp aTimestamp,
                                         int64_t aTimestampJS,
                                         uint64_t aFrameNumber)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  MOZ_ASSERT(aTimestampNanosecond > mCurrentTimestampNanosecond);
  MOZ_ASSERT(aTimestamp > mCurrentTimestamp);
  MOZ_ASSERT(aTimestampJS > mCurrentTimestampJS);
  mCurrentTimestampNanosecond = aTimestampNanosecond;
  mCurrentTimestamp = aTimestamp;
  mCurrentTimestampJS = aTimestampJS;
  mCurrentFrameNumber = aFrameNumber;

  // End
  char propValue[PROPERTY_VALUE_MAX];
  property_get("silk.hw2vsync", propValue, "0");
  if (atoi(propValue) != 0) {
    property_get("silk.timer.log", propValue, "0");
    if (atoi(propValue) == 0) {
      VSYNC_ASYNC_SYSTRACE_LABEL_END_PRINTF((int32_t)mCurrentFrameNumber, "HwToVsync (%u)", (uint32_t)mCurrentFrameNumber);
    }
    else {
      static VsyncLatencyLogger* logger = VsyncLatencyLogger::CreateLogger("Silk Host::NotifyVsyncTask");
      TimeDuration diff = TimeStamp::Now() - aTimestamp;
      logger->Update(aFrameNumber, (int64_t)diff.ToMicroseconds());
      if(!(aFrameNumber % 256)){
        logger->PrintStatistic();
        logger->Reset();
      }
    }
  }

  property_get("silk.vd", propValue, "0");
  if (atoi(propValue) != 0) {
    VSYNC_SCOPED_SYSTRACE_LABEL_PRINTF("DispatchVsync (%u)", (uint32_t)mCurrentFrameNumber);
    DispatchVsyncEvent();
  } else {
    DispatchVsyncEvent();
  }
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
  DispatchCompose();

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

  return mVsyncEventParentList.Length() + mInputDispatcher->GetObserverNum()
                                        + mRefreshDriver->GetObserverNum()
                                        + mCompositor->GetObserverNum();
}

void
VsyncDispatcherHostImpl::DispatchInputEvent()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mInputDispatcher->Dispatch(mCurrentTimestampNanosecond,
                             mCurrentTimestamp,
                             mCurrentTimestampJS,
                             mCurrentFrameNumber);
}

void
VsyncDispatcherHostImpl::DispatchCompose()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mCompositor->Dispatch(mCurrentTimestampNanosecond,
                        mCurrentTimestamp,
                        mCurrentTimestampJS,
                        mCurrentFrameNumber);
}

void
VsyncDispatcherHostImpl::TickRefreshDriver()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  mRefreshDriver->Dispatch(mCurrentTimestampNanosecond,
                           mCurrentTimestamp,
                           mCurrentTimestampJS,
                           mCurrentFrameNumber);
}

void
VsyncDispatcherHostImpl::NotifyContentProcess()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  VsyncData vsyncData(mCurrentTimestampNanosecond,
                      mCurrentTimestamp,
                      mCurrentTimestampJS,
                      mCurrentFrameNumber);

  // Send ipc to content process.
  for (VsyncEventParentList::size_type i = 0; i < mVsyncEventParentList.Length(); ++i) {
    VsyncEventParent* parent = mVsyncEventParentList[i];

    char propValue[PROPERTY_VALUE_MAX];
    property_get("silk.ipc", propValue, "0");
    if (atoi(propValue) != 0) {
      VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN_PRINTF((int32_t)mCurrentFrameNumber, "IPC (%u)", (uint32_t)mCurrentFrameNumber);
    }

    if (!parent->SendNotifyVsyncEvent(vsyncData)) {
      NS_WARNING("Send Notify Vsync Event Error");
    }
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
  MOZ_ASSERT(mTimer);
  MOZ_ASSERT(IsInVsyncDispatcherThread());

  // We check the observer number here to enable/disable vsync event
  if (!!GetVsyncObserverCount() !=  mVsyncEventNeeded) {
    mVsyncEventNeeded = !mVsyncEventNeeded;

    mTimer->Enable(mVsyncEventNeeded);
  }
}

bool
VsyncDispatcherHostImpl::IsInVsyncDispatcherThread() const
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
