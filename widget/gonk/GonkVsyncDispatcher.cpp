/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkVsyncDispatcher.h"
#include "gfxPrefs.h"
#include "HwcComposer2D.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#define LOGI(args...) __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "Gonk", ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "Gonk", ## args)

namespace mozilla {

static void
VsyncCallback(void)
{
  VsyncDispatcherHost::GetInstance()->NotifyVsync(base::TimeTicks::HighResNow().ToInternalValue());
}

GonkVsyncDispatcher::GonkVsyncDispatcher()
  : mVsyncInited(false)
  , mUseHWVsync(false)
{

}

GonkVsyncDispatcher::~GonkVsyncDispatcher()
{

}

void
GonkVsyncDispatcher::StartUpVsyncEvent()
{
  MOZ_ASSERT(NS_IsMainThread(), "StartUpVsyncEvent should be called in main thread.");
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default,
      "StartUpVsyncEvent should be called in chrome");
  MOZ_ASSERT(!mVsyncInited, "VsyncEvent is already initialized.");

  mVsyncInited = true;

  // Use hw event or software vsync event
  if (gfxPrefs::FrameUniformityHWVsyncEnabled()) {
    // init hw vsync event
    if (HwcComposer2D::GetInstance()->RegisterHwcEventCallback()) {
      mVsyncRate = HwcComposer2D::GetInstance()->GetHWVsycnRate();
      HwcComposer2D::GetInstance()->RegisterVsyncCallback(VsyncCallback);
      HwcComposer2D::GetInstance()->EnableVsync(true);
      mUseHWVsync = true;
      LOGI("GonkVsyncDispatcher: use hwc vsync");
    }
  }
  // Fallback to software vsync event
  if (!mUseHWVsync) {
    //TODO: init software vsync event here.
    LOGI("GonkVsyncDispatcher: use soft vsync");
  }
}

void
GonkVsyncDispatcher::ShutDownVsyncEvent()
{
  MOZ_ASSERT(NS_IsMainThread(), "ShutDownVsyncEvent should at main thread");
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default,
      "ShutDownVsyncEvent should be called in chrome");
  MOZ_ASSERT(mVsyncInited, "VsyncEvent is not initialized.");

  if (mUseHWVsync) {
    HwcComposer2D::GetInstance()->UnregisterVsyncCallback();
  }
  else {
    //TODO: release software vsync event here.
  }
  mVsyncInited = false;
}

void
GonkVsyncDispatcher::EnableVsyncEvent(bool aEnable)
{
  MOZ_ASSERT(IsInVsyncDispatcherHostThread(), "Call EnableVsyncEvent at wrong thread");
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default,
      "EnableVsyncEvent should be called in chrome");
  MOZ_ASSERT(mVsyncInited, "VsyncEvent is not initialized");

  if (mUseHWVsync) {
    HwcComposer2D::GetInstance()->EnableVsync(aEnable);
  }
  else {
    //TODO: enable/disable the software vsync event here.
  }
}

} // namespace mozilla
