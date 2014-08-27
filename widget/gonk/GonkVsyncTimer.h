/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_gonk_GonkVsyncTimer_h
#define mozilla_widget_gonk_GonkVsyncTimer_h

#include "mozilla/Attributes.h"
#include "VsyncPlatformTimer.h"

namespace mozilla {

class GonkVsyncTimer : public VsyncPlatformTimer
{
public:
  GonkVsyncTimer();
  ~GonkVsyncTimer();

  virtual void SetObserver(VsyncTimerObserver* aObserver) MOZ_OVERRIDE;

  virtual void Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;
  virtual void Enable(bool aEnable) MOZ_OVERRIDE;

  virtual uint32_t GetVsyncRate() MOZ_OVERRIDE;

private:
  VsyncTimerObserver* mObserver;

  bool mInited;
  bool mUseHWVsync;
  uint32_t mVsyncRate;
};

} // namespace mozilla

#endif //mozilla_widget_gonk_GonkVsyncTimer_h
