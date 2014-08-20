/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VsyncTimer
#define mozilla_VsyncTimer

#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "PlatformVsyncTimer.h"

namespace mozilla {

class VsyncRunnable;

class VsyncTimer MOZ_FINAL : public nsISupports
                           , public PlatformVsyncTimer
{
  NS_DECL_THREADSAFE_ISUPPORTS;

public:
  VsyncTimer(VsyncTimerObserver* aObserver);
  virtual ~VsyncTimer();

  virtual void Shutdown() MOZ_OVERRIDE;
  virtual void Enable(bool aEnable) MOZ_OVERRIDE;
  virtual uint32_t GetVsyncRate() MOZ_OVERRIDE;
  virtual void SetPriority(int32_t aPriority) MOZ_OVERRIDE;

private:
  virtual bool Startup() MOZ_OVERRIDE;

  nsCOMPtr<nsIThread> mTimerThread;
  nsRefPtr<VsyncRunnable> mVsyncRunnable;
};

} // namespace mozilla

#endif // mozilla_VsyncTimer
