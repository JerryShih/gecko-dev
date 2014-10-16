/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_gonk_GonkVsyncTimer_h
#define mozilla_widget_gonk_GonkVsyncTimer_h

#include "mozilla/Attributes.h"
#include "PlatformVsyncTimer.h"

namespace mozilla {

class HwcComposer2D;

class GonkVsyncTimer MOZ_FINAL : public PlatformVsyncTimer
{
  friend class HwcComposer2D;
public:
  GonkVsyncTimer(VsyncTimerObserver* aObserver);
  ~GonkVsyncTimer();

  virtual bool Startup() MOZ_OVERRIDE;
  virtual void Shutdown() MOZ_OVERRIDE;
  virtual void Enable(bool aEnable) MOZ_OVERRIDE;

private:
  // Called by HwcComposer2D if a vsync event comes.
  void NotifyVsync(TimeStamp aTimestamp);

  bool mInited;
};

} // namespace mozilla

#endif //mozilla_widget_gonk_GonkVsyncTimer_h
