/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GonkVsyncDispatcher_h
#define mozilla_GonkVsyncDispatcher_h

#include "mozilla/VsyncDispatcherHost.h"

namespace mozilla {

class GonkVsyncDispatcher MOZ_FINAL : public VsyncDispatcherHost
{
  friend class VsyncDispatcherHost;

private:
  // Singleton pattern. Hide constructor and destructor.
  GonkVsyncDispatcher();
  ~GonkVsyncDispatcher();

  virtual void StartUpVsyncEvent() MOZ_OVERRIDE;
  virtual void ShutDownVsyncEvent() MOZ_OVERRIDE;
  virtual void EnableVsyncEvent(bool aEnable) MOZ_OVERRIDE;

  // vsync event generator.
  bool mVsyncInited;
  bool mUseHWVsync;
};

} // namespace mozilla

#endif // mozilla_GonkVsyncDispatcher_h
