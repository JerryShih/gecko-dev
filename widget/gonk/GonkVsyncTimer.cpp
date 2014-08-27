/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkVsyncTimer.h"
#include "gfxPrefs.h"
#include "HwcComposer2D.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#define LOGI(args...) __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "Gonk", ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "Gonk", ## args)

namespace mozilla {

GonkVsyncTimer::GonkVsyncTimer()
  : mObserver(nullptr)
  , mInited(false)
  , mUseHWVsync(false)
  , mVsyncRate(0)
{

}

GonkVsyncTimer::~GonkVsyncTimer()
{

}

void
GonkVsyncTimer::SetObserver(VsyncTimerObserver* aObserver)
{
  MOZ_ASSERT(!mInited);
  MOZ_ASSERT(!mObserver);

  mObserver = aObserver;
}

void
GonkVsyncTimer::Startup()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(!mInited);
  MOZ_ASSERT(mObserver);

  mInited = true;

  // Use hw event or software vsync event
  if (gfxPrefs::FrameUniformityHWVsyncEnabled()) {
    // init hw vsync event
    if (HwcComposer2D::GetInstance()->InitHwcEventCallback()) {
      mVsyncRate = HwcComposer2D::GetInstance()->GetHWVsyncRate();
      HwcComposer2D::GetInstance()->RegisterVsyncObserver(mObserver);
      HwcComposer2D::GetInstance()->EnableVsync(true);
      mUseHWVsync = true;
      LOGI("GonkVsyncTimer: use hwc vsync");
    }
  }
  // Fallback to software vsync event
  if (!mUseHWVsync) {
    //TODO: init software vsync event here.
    LOGI("GonkVsyncTimer: use soft vsync");
  }
}

void
GonkVsyncTimer::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  if (mUseHWVsync) {
    HwcComposer2D::GetInstance()->UnregisterVsyncObserver();
  } else {
  //TODO: release software vsync event here.
  }
}

void
GonkVsyncTimer::Enable(bool aEnable)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  if (mUseHWVsync) {
    HwcComposer2D::GetInstance()->EnableVsync(aEnable);
  }
  else {
    //TODO: enable/disable the software vsync event here.
  }
}

uint32_t
GonkVsyncTimer::GetVsyncRate()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mVsyncRate);

  return mVsyncRate;
}

} // namespace mozilla
