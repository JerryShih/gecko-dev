/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkVsyncDispatcher.h"

#include "mozilla/layers/VsyncEventParent.h"
#include "mozilla/layers/VsyncEventChild.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "gfxPrefs.h"
#include "HwcComposer2D.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

//#define DEBUG_VSYNC
#ifdef DEBUG_VSYNC
#define VSYNC_PRINT(...) do { printf_stderr("bignose " __VA_ARGS__); } while (0)
#else
#define VSYNC_PRINT(...) do { } while (0)
#endif


namespace mozilla {

using namespace layers;

static base::Thread* sVsyncDispatchThread = nullptr;
static MessageLoop* sVsyncDispatchMessageLoop = nullptr;

static scoped_refptr<GonkVsyncDispatcher> sGonkVsyncDispatcher;

class ArrayDataHelper
{
public:
  template <typename Type>
  static void Add(nsTArray<Type*>* aList, Type* aItem)
  {
    //MOZ_RELEASE_ASSERT(!aList->Contains(aItem));
    //MOZ_ASSERT(!aList->Contains(aItem));
    if (!aList->Contains(aItem)) {
      aList->AppendElement(aItem);
    }

    GonkVsyncDispatcher::GetInstance()->CheckVsyncNotification();
  }

  template <typename Type>
  static void Remove(nsTArray<Type*>* aList, Type* aItem)
  {
    typedef nsTArray<Type*> ArrayType;
    typename ArrayType::index_type index = aList->IndexOf(aItem);

    //MOZ_RELEASE_ASSERT(index != ArrayType::NoIndex);
    //MOZ_ASSERT(index != ArrayType::NoIndex);
    if (index != ArrayType::NoIndex) {
      aList->RemoveElementAt(index);
    }

    GonkVsyncDispatcher::GetInstance()->CheckVsyncNotification();
  }
};

static bool
CreateThread()
{
  if (sVsyncDispatchThread) {
    return true;
  }

  sVsyncDispatchThread = new base::Thread("Vsync dispatch thread");

  if (!sVsyncDispatchThread->Start()) {
    delete sVsyncDispatchThread;
    sVsyncDispatchThread = nullptr;
    return false;
  }

  sVsyncDispatchMessageLoop = sVsyncDispatchThread->message_loop();

  return true;
}

/*static*/ GonkVsyncDispatcher*
GonkVsyncDispatcher::GetInstance()
{
  return sGonkVsyncDispatcher;
}

/*static*/ void
GonkVsyncDispatcher::StartUp()
{
  // we only call startup at main thread
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  //MOZ_ASSERT(NS_IsMainThread());
  // only b2g need to create a new thread
  MOZ_RELEASE_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  //MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  CreateThread();

  // We should have message loop before calling GetInstance()
  MOZ_RELEASE_ASSERT(sVsyncDispatchMessageLoop);
  //MOZ_ASSERT(sVsyncDispatchMessageLoop);

  if (sGonkVsyncDispatcher == nullptr) {
    sGonkVsyncDispatcher = new GonkVsyncDispatcher();

    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      sGonkVsyncDispatcher->StartUpVsyncEvent();
    }
  }
}

/*static*/ void
GonkVsyncDispatcher::StartUpOnCurrentThread(void)
{
  // we only call startup at main thread
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  //MOZ_ASSERT(NS_IsMainThread());
  //only content process should use the existed message loop
  MOZ_RELEASE_ASSERT(XRE_GetProcessType() != GeckoProcessType_Default);
  //MOZ_ASSERT(XRE_GetProcessType() != GeckoProcessType_Default);

  if (!sVsyncDispatchMessageLoop) {
    sVsyncDispatchMessageLoop = MessageLoop::current();
  }

  if (sGonkVsyncDispatcher == nullptr) {
    sGonkVsyncDispatcher = new GonkVsyncDispatcher();
  }
}

/*static*/ void
GonkVsyncDispatcher::ShutDown()
{
  // we only call shutdown at main thread
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  //MOZ_ASSERT(NS_IsMainThread());

  if (sGonkVsyncDispatcher) {
    MOZ_RELEASE_ASSERT(sVsyncDispatchMessageLoop);
    //MOZ_ASSERT(sVsyncDispatchMessageLoop);
    if (sVsyncDispatchMessageLoop) {
      // PostTask will call addRef, so we can assign nullptr after post task.
      sVsyncDispatchMessageLoop->PostTask(FROM_HERE,
                                          NewRunnableMethod(sGonkVsyncDispatcher.get(),
                                          &GonkVsyncDispatcher::EnableVsyncDispatch,
                                          false));

      if (XRE_GetProcessType() == GeckoProcessType_Default) {
        sGonkVsyncDispatcher->ShutDownVsyncEvent();
      }
      sGonkVsyncDispatcher = nullptr;
      sVsyncDispatchMessageLoop = nullptr;
    }
  }

  if (sVsyncDispatchThread) {
    delete sVsyncDispatchThread;
    sVsyncDispatchThread = nullptr;
  }
}

GonkVsyncDispatcher::GonkVsyncDispatcher()
  : mVsyncEventChild(nullptr)
  , mEnableVsyncDispatch(true)
  , mNeedVsyncEvent(false)
  , mInitVsyncEventGenerator(false)
  , mUseHWVsyncEventGenerator(false)
{
  printf_stderr("bignose TestSilk GonkVsyncDispatcher::GonkVsyncDispatcher:%p, tid:%d",this,gettid());
}

GonkVsyncDispatcher::~GonkVsyncDispatcher()
{
  printf_stderr("bignose TestSilk GonkVsyncDispatcher::GonkVsyncDispatcher:%p, tid:%d",this,gettid());
}

void
GonkVsyncDispatcher::StartUpVsyncEvent()
{
  printf_stderr("bignose TestSilk GonkVsyncDispatcher::StartUpVsyncEvent, tid:%d", gettid());

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    if (!mInitVsyncEventGenerator){
      mInitVsyncEventGenerator = true;
      mUseHWVsyncEventGenerator = false;

#if ANDROID_VERSION >= 17
      // check using hw event or software vsync event
      if (gfxPrefs::SilkHWVsyncEnabled()) {
        //init hw vsync event
        HwcComposer2D::GetInstance()->InitHwcEventCallback();
        if (HwcComposer2D::GetInstance()->HasHWVsync()) {
          HwcComposer2D::GetInstance()->RegisterVsyncDispatcher(sGonkVsyncDispatcher);
          mUseHWVsyncEventGenerator = true;
        }
      }
#endif
      if (!mUseHWVsyncEventGenerator) {
        //TODO:
        //init software vsync event here.
      }
    }
  }
}

void
GonkVsyncDispatcher::ShutDownVsyncEvent()
{
  printf_stderr("bignose TestSilk GonkVsyncDispatcher::ShutDownVsyncEvent, tid:%d", gettid());

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    if (mInitVsyncEventGenerator) {
#if ANDROID_VERSION >= 17
      if (mUseHWVsyncEventGenerator) {
        HwcComposer2D::GetInstance()->ShutDownHwcEvent();
      }
#endif
      if (!mUseHWVsyncEventGenerator) {
        //TODO:
        //release software vsync event here.
      }
    }
  }
}

void
GonkVsyncDispatcher::SetVsyncEventChild(VsyncEventChild* aVsyncEventChild)
{
  mVsyncEventChild = aVsyncEventChild;
}

void
GonkVsyncDispatcher::EnableVsyncDispatch(bool aEnable)
{
  mEnableVsyncDispatch = aEnable;
}

int
GonkVsyncDispatcher::GetRegistedObjectCount() const
{
   int count = 0;

   count += mVsyncEventParentList.Length();

   return count;
}

void
GonkVsyncDispatcher::EnableVsyncEvent(bool aEnable)
{
  if (mInitVsyncEventGenerator) {
    if (mUseHWVsyncEventGenerator) {
      HwcComposer2D::GetInstance()->EnableVsync(aEnable);
    }
    else {
      //TODO:
      //enable/disable software vsync event here.
    }
  }
}

void
GonkVsyncDispatcher::CheckVsyncNotification()
{
  if (!!GetRegistedObjectCount() !=  mNeedVsyncEvent) {
    mNeedVsyncEvent = !mNeedVsyncEvent;

    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      EnableVsyncEvent(mNeedVsyncEvent);
    }
    else{
      if (mNeedVsyncEvent) {
        mVsyncEventChild->SendEnableVsyncEventNotification();
      }
      else {
        mVsyncEventChild->SendDisableVsyncEventNotification();
      }
    }
  }
}

MessageLoop*
GonkVsyncDispatcher::GetMessageLoop()
{
  MOZ_RELEASE_ASSERT(sVsyncDispatchMessageLoop);
  //MOZ_ASSERT(sVsyncDispatchMessageLoop);

  return sVsyncDispatchMessageLoop;
}

void
GonkVsyncDispatcher::NotifyVsync(int64_t aTimestamp)
{
  GetMessageLoop()->PostTask(FROM_HERE,
                             NewRunnableMethod<GonkVsyncDispatcher, void(GonkVsyncDispatcher::*)(int64_t)>(this,
                             &GonkVsyncDispatcher::DispatchVsyncEvent,
                             aTimestamp));
}

void
GonkVsyncDispatcher::DispatchVsyncEvent(int64_t aTimestamp)
{
  static uint32_t frameNumber = 0;

  DispatchVsyncEvent(aTimestamp, ++frameNumber);
}

void
GonkVsyncDispatcher::DispatchVsyncEvent(int64_t aTimestamp, uint32_t aFrameNumber)
{
  VSYNC_PRINT("DispatchVsyncEvent, time:%lld, frame:%u", aTimestamp, aFrameNumber);

  if (!mEnableVsyncDispatch || !mNeedVsyncEvent) {
    return;
  }

  // TODO: dispatch vsync event to other module

  // Send vsync event to content process
  NotifyVsyncEventChild(aTimestamp, aFrameNumber);
}

void
GonkVsyncDispatcher::RegisterVsyncEventParent(VsyncEventParent* aVsyncEventParent)
{
  VSYNC_PRINT("RegisterVsyncEventParent");

  GetMessageLoop()->PostTask(FROM_HERE,
                             NewRunnableFunction(&ArrayDataHelper::Add<VsyncEventParent>,
                             &mVsyncEventParentList,
                             aVsyncEventParent));
}

void
GonkVsyncDispatcher::UnregisterVsyncEventParent(VsyncEventParent* aVsyncEventParent)
{
  VSYNC_PRINT("UnregisterVsyncEventParent");

  GetMessageLoop()->PostTask(FROM_HERE,
                             NewRunnableFunction(&ArrayDataHelper::Remove<VsyncEventParent>,
                             &mVsyncEventParentList,
                             aVsyncEventParent));
}

void
GonkVsyncDispatcher::NotifyVsyncEventChild(int64_t aTimestamp, uint32_t aFrameNumber)
{
  VsyncData vsyncData(aTimestamp, aFrameNumber);

  // Tick all registered content process.
  for (VsyncEventParentList::size_type i = 0; i < mVsyncEventParentList.Length(); i++) {
    VsyncEventParent* parent = mVsyncEventParentList[i];
    parent->SendNotifyVsyncEvent(vsyncData);
  }
}

} // namespace mozilla
