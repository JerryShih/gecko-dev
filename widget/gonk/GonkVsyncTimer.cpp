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

}

GonkVsyncTimer::~GonkVsyncTimer()
{

}

bool
GonkVsyncTimer::Startup()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(!mInited);
  MOZ_ASSERT(mObserver);

  bool result = false;

  mInited = true;

  if (gfxPrefs::FrameUniformityHWVsyncEnabled()) {
    if (HwcComposer2D::GetInstance()->InitHwcEventCallback()) {
      mVsyncRate = HwcComposer2D::GetInstance()->GetHWVsyncRate();
      HwcComposer2D::GetInstance()->RegisterVsyncObserver(mObserver);
      result = true;

      LOGI("GonkVsyncTimer: use hwc vsync");
    }
  }

  return result;
}

void
GonkVsyncTimer::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  HwcComposer2D::GetInstance()->UnregisterVsyncObserver();
}

void
GonkVsyncTimer::Enable(bool aEnable)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mInited);

  HwcComposer2D::GetInstance()->EnableVsync(aEnable);
}

uint32_t
GonkVsyncTimer::GetVsyncRate()
{
  MOZ_ASSERT(mInited);
  MOZ_ASSERT(mVsyncRate);

  return mVsyncRate;
}

} // namespace mozilla
