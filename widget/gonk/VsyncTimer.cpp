/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncTimer.h"

#include <errno.h>

#include "jsapi.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "nsThread.h"
#include "utils/Timers.h"

#define VSYNC_FREQ 16666666 // ns (60 Hz)
#define NANO_SEC 1000000000 // 10^9 ns

namespace mozilla {

class VsyncRunnable : public nsCancelableRunnable
{
// Methods
public:
  VsyncRunnable(nsecs_t aPeriod, bool aErrorCorrection = false);
  virtual ~VsyncRunnable();
  double GetRate() const;
  void SetEnabled(bool aEnabled);
  void DisableWithBlock();
  void RegisterVsyncObserver(VsyncTimerObserver* aObserver);

  NS_IMETHOD Run() MOZ_OVERRIDE;
  NS_IMETHOD Cancel() MOZ_OVERRIDE;

private:
  VsyncRunnable();
  void NotifyBlockingMonitor();
  bool WaitForEnable();
  bool RunLoop();

// Data members
private:
  bool mEnabled;
  bool mRunning;
  bool mErrorCorrection;

  // Units: ns
  nsecs_t mRefreshPeriod;
  nsecs_t mNextFakeVsync;

  Monitor mMonitor;
  Monitor mBlockingMonitor;
  VsyncTimerObserver* mVsyncObserver;
};


// ----------------------------------------------
// VsyncRunnable Implementation
// ----------------------------------------------
VsyncRunnable::VsyncRunnable(nsecs_t aPeriod, bool aErrorCorrection)
  : mEnabled(false)
  , mRunning(true)
  , mErrorCorrection(aErrorCorrection)
  , mRefreshPeriod(aPeriod)
  , mNextFakeVsync(0)
  , mMonitor("VsyncRunnableMonitor")
  , mBlockingMonitor("VsyncRunnableBlockingMonitor")
  , mVsyncObserver(nullptr)
{
}

VsyncRunnable::~VsyncRunnable()
{
}

double
VsyncRunnable::GetRate() const
{
  return (double)NANO_SEC/(double)mRefreshPeriod;
}

void
VsyncRunnable::SetEnabled(bool aEnabled)
{
  MonitorAutoLock lock(mMonitor);
  if (mEnabled != aEnabled) {
    mEnabled = aEnabled;
    mMonitor.Notify();
  }
}

void
VsyncRunnable::DisableWithBlock()
{
  // This is a special disable function which uses
  // mBlockingMonitor to wait for the finish of the final loop.
  // Set a timer wait (twice as mRefreshPeriod) to prevent from
  // forever wait.
  MonitorAutoLock lock(mMonitor);
  if (mEnabled) {
    mEnabled = false;
    MonitorAutoLock blockLock(mBlockingMonitor);
    // (mRefreshPeriod * 2 / 1000) us
    mBlockingMonitor.Wait(PR_MicrosecondsToInterval(mRefreshPeriod/500));
  }
}

void
VsyncRunnable::RegisterVsyncObserver(VsyncTimerObserver* aObserver)
{
  mVsyncObserver = aObserver;
}

NS_IMETHODIMP
VsyncRunnable::Run()
{
  while (RunLoop()) {}
  return NS_OK;
}

NS_IMETHODIMP
VsyncRunnable::Cancel()
{
  mRunning = false;
  MonitorAutoLock lock(mMonitor);
  mMonitor.Notify();
  return NS_OK;
}

void
VsyncRunnable::NotifyBlockingMonitor()
{
  MonitorAutoLock lock(mBlockingMonitor);
  mBlockingMonitor.Notify();
}

bool
VsyncRunnable::WaitForEnable()
{
  NotifyBlockingMonitor();
  MonitorAutoLock lock(mMonitor);
  while (!mEnabled && mRunning) {
    mMonitor.Wait();
  }
  return mRunning;
}

bool
VsyncRunnable::RunLoop()
{
  if (!WaitForEnable()) {
    return false;
  }

  // Calculate sleep interval
  const nsecs_t period = mRefreshPeriod;
  const nsecs_t now = systemTime(CLOCK_MONOTONIC);
  nsecs_t sleep = period;

  if (mErrorCorrection) {
    nsecs_t nextVsync = mNextFakeVsync;
    sleep = nextVsync - now;
    if (sleep < 0) {
      // we missed, so find where the next vsync should be
      sleep = (period - ((now - nextVsync) % period));
      nextVsync = now + sleep;
    }
    mNextFakeVsync = nextVsync + period;
  }

  struct timespec spec;
  spec.tv_sec  = sleep / NANO_SEC;
  spec.tv_nsec = sleep % NANO_SEC;

  int err = 0;
  do {
    err = nanosleep(&spec, nullptr);
  } while (err < 0 && errno == EINTR);

  if (mVsyncObserver && err == 0) {
    static nsecs_t last = now;
    int64_t diff = now - last;
    static uint64_t count = 0;
    //printf_stderr("[Boris] SW Vsync (diff: %lld, freq: %lfHz)",diff, (double)NANO_SEC/(double)diff);
    if (diff > 19000000 && diff < 100000000) {
      ++count;
      printf_stderr("[Boris] diff is too much (%llu) (diff: %lld)",count, diff);
    }
    mVsyncObserver->NotifyVsync(now, TimeStamp::Now(), JS_Now());
    last = now;
  }

  return true;
}


// ----------------------------------------------
// VsyncTimer Implementation
// ----------------------------------------------
NS_IMPL_ISUPPORTS(VsyncTimer, nsISupports);

VsyncTimer::VsyncTimer(VsyncTimerObserver* aObserver)
  : PlatformVsyncTimer(aObserver)
  , mVsyncRunnable(new VsyncRunnable(VSYNC_FREQ))
{
  mVsyncRunnable->RegisterVsyncObserver(mObserver);
}

VsyncTimer::~VsyncTimer()
{
  if (mVsyncRunnable && mTimerThread) {
    mVsyncRunnable->DisableWithBlock();
    mVsyncRunnable->Cancel();
    mTimerThread->Shutdown();
  }
}

bool
VsyncTimer::Startup()
{
  nsresult rv = NS_NewNamedThread("SW Vsync",
                                  getter_AddRefs(mTimerThread),
                                  mVsyncRunnable);

  return NS_SUCCEEDED(rv);
}

void
VsyncTimer::Shutdown()
{
  MOZ_ASSERT(mVsyncRunnable && mTimerThread);

  if (mVsyncRunnable) {
    mVsyncRunnable->DisableWithBlock();
  }
}

void
VsyncTimer::Enable(bool aEnable)
{
  MOZ_ASSERT(mVsyncRunnable && mTimerThread);

  if (mVsyncRunnable) {
    mVsyncRunnable->SetEnabled(aEnable);
  }
}

uint32_t
VsyncTimer::GetVsyncRate()
{
  MOZ_ASSERT(mVsyncRunnable && mTimerThread);

  uint32_t ret = 60;
  if (mVsyncRunnable) {
    ret = mVsyncRunnable->GetRate();
  }

  return ret;
}

void
VsyncTimer::SetPriority(int32_t aPriority)
{
  if (mTimerThread) {
    nsThread* thread = static_cast<nsThread*>(mTimerThread.get());
    DebugOnly<nsresult> rv = thread->SetPriority(aPriority);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to set priority");
  }
}

} // namespace mozilla
