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

GonkVsyncTimer::GonkVsyncTimer(VsyncTimerObserver* aObserver)
  : PlatformVsyncTimer(aObserver)
  , mInited(false)
{
  MOZ_ASSERT(aObserver);
}

GonkVsyncTimer::~GonkVsyncTimer()
{

}

void
GonkVsyncTimer::NotifyVsync(TimeStamp aTimestamp)
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mObserver);

  mObserver->NotifyVsync(aTimestamp);
}

bool
GonkVsyncTimer::Startup()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(!mInited);
  MOZ_ASSERT(mObserver);

  if (gfxPrefs::FrameUniformityHWVsyncEnabled()) {
    if (HwcComposer2D::GetInstance()->InitHwcEventCallback()) {
      HwcComposer2D::GetInstance()->RegisterVsyncTimer(this);
      mInited = true;

      LOGI("GonkVsyncTimer: use hwc vsync");
    }
  }

  return mInited;
}

void
GonkVsyncTimer::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  HwcComposer2D::GetInstance()->UnregisterVsyncTimer();
}

void
GonkVsyncTimer::Enable(bool aEnable)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  HwcComposer2D::GetInstance()->EnableVsync(aEnable);
}

} // namespace mozilla
