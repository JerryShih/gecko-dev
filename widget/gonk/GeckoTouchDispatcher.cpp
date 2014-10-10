/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* Copyright 2014 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FrameMetrics.h"
#include "GeckoProfiler.h"
#include "GeckoTouchDispatcher.h"
#include "InputData.h"
#include "ProfilerMarkers.h"
#include "base/basictypes.h"
#include "gfxPrefs.h"
#include "libui/Input.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/dom/Touch.h"
#include "nsAppShell.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "nsWindow.h"
#include <sys/types.h>
#include <unistd.h>

// uncomment to print log resample data
// #define LOG_RESAMPLE_DATA 1

#ifdef LOG_RESAMPLE_DATA
#define LOG(args...)                                            \
  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#else
#define LOG(args...)
#endif


namespace mozilla {

// Amount of time in MS before an input is considered expired.
static const uint64_t kInputExpirationThresholdMs = 1000;
static StaticRefPtr<GeckoTouchDispatcher> sTouchDispatcher;

GeckoTouchDispatcher::GeckoTouchDispatcher()
  : mTouchQueueLock("GeckoTouchDispatcher::mTouchQueueLock")
  , mTouchEventsFiltered(false)
{
  // Since GeckoTouchDispatcher is initialized when input is initialized
  // and reads gfxPrefs, it is the first thing to touch gfxPrefs.
  // The first thing to touch gfxPrefs MUST occur on the main thread and init
  // the singleton
  MOZ_ASSERT(sTouchDispatcher == nullptr);
  MOZ_ASSERT(NS_IsMainThread());
  gfxPrefs::GetSingleton();

  mEnabledUniformityInfo = gfxPrefs::UniformityInfo();
  mResamplingEnabled = gfxPrefs::TouchResampling() &&
                       gfxPrefs::FrameUniformityHWVsyncEnabled();
  mVsyncAdjust = TimeDuration::FromMilliseconds(gfxPrefs::TouchVsyncSampleAdjust());
  mMaxPredict = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleMaxPredict());
  mMinResampleTime = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleMinTime());
  mDelayedVsyncThreshold = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleVsyncDelayThreshold());
  mOldTouchThreshold = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleOldTouchThreshold());
  sTouchDispatcher = this;
  ClearOnShutdown(&sTouchDispatcher);
}

class DispatchTouchEventsMainThread : public nsRunnable
{
public:
  DispatchTouchEventsMainThread(GeckoTouchDispatcher* aTouchDispatcher,
                                TimeStamp aVsyncTime)
    : mTouchDispatcher(aTouchDispatcher)
    , mVsyncTime(aVsyncTime)
  {
  }

  NS_IMETHOD Run()
  {
    mTouchDispatcher->DispatchTouchMoveEvents(mVsyncTime);
    return NS_OK;
  }

private:
  nsRefPtr<GeckoTouchDispatcher> mTouchDispatcher;
  TimeStamp mVsyncTime;
};

class DispatchSingleTouchMainThread : public nsRunnable
{
public:
  DispatchSingleTouchMainThread(GeckoTouchDispatcher* aTouchDispatcher,
                                MultiTouchInput& aTouch)
    : mTouchDispatcher(aTouchDispatcher)
    , mTouch(aTouch)
  {
  }

  NS_IMETHOD Run()
  {
    mTouchDispatcher->DispatchTouchEvent(mTouch);
    return NS_OK;
  }

private:
  nsRefPtr<GeckoTouchDispatcher> mTouchDispatcher;
  MultiTouchInput mTouch;
};

// Timestamp is in nanoseconds
/* static */ bool
GeckoTouchDispatcher::NotifyVsync(TimeStamp aVsyncTime)
{
  if (sTouchDispatcher == nullptr) {
    return false;
  }

  MOZ_ASSERT(sTouchDispatcher->mResamplingEnabled);
  bool haveTouchData = false;
  {
    MutexAutoLock lock(sTouchDispatcher->mTouchQueueLock);
    haveTouchData = !sTouchDispatcher->mTouchMoveEvents.empty();
  }

  if (haveTouchData) {
    NS_DispatchToMainThread(new DispatchTouchEventsMainThread(sTouchDispatcher,
                            aVsyncTime));
  }

  return haveTouchData;
}

void
GeckoTouchDispatcher::NotifyTouch(MultiTouchInput aTouch)
{
  if (aTouch.mType == MultiTouchInput::MULTITOUCH_MOVE) {
    MutexAutoLock lock(mTouchQueueLock);
    if (mResamplingEnabled || mTouchMoveEvents.empty()) {
      mTouchMoveEvents.push_back(aTouch);
      if (mResamplingEnabled) {
        return;
      }
    } else {
      mTouchMoveEvents.back() = aTouch;
    }

    NS_DispatchToMainThread(new DispatchTouchEventsMainThread(sTouchDispatcher,
                            TimeStamp::Now()));
  } else {
    NS_DispatchToMainThread(new DispatchSingleTouchMainThread(this, aTouch));
  }
}

void
GeckoTouchDispatcher::DispatchTouchMoveEvents(TimeStamp aVsyncTime)
{
  MultiTouchInput touchMove;

  {
    MutexAutoLock lock(mTouchQueueLock);
    if (mTouchMoveEvents.empty()) {
      return;
    }

    if (!mResamplingEnabled) {
      touchMove = mTouchMoveEvents.back();
      mTouchMoveEvents.clear();
    } else {
      int touchCount = mTouchMoveEvents.size();
      TimeStamp lastTouchTime = mTouchMoveEvents.back().mTimeStamp;
      TimeDuration vsyncTouchDiff = aVsyncTime - lastTouchTime;
      bool isDelayedVsyncEvent = vsyncTouchDiff < mDelayedVsyncThreshold;
      bool isOldTouch = vsyncTouchDiff > mOldTouchThreshold;

      MultiTouchInput aFirstTouch;
      MultiTouchInput aSecondTouch;
      bool resample = GetTouchEvents(aFirstTouch, aSecondTouch, aVsyncTime)
                                    && !isDelayedVsyncEvent && !isOldTouch;

      LOG("Resampling: %d, touch count: %d, delayed vsync: %d, old touch: %d, vsync touch diff: %f\n",
          resample, touchCount, isDelayedVsyncEvent, isOldTouch, vsyncTouchDiff.ToMilliseconds());

      if (!resample) {
        touchMove = mTouchMoveEvents.back();
        mTouchMoveEvents.clear();
        if (!isDelayedVsyncEvent && !isOldTouch) {
          mTouchMoveEvents.push_back(touchMove);
        }

        ScreenIntPoint singleTouch = touchMove.mTouches[0].mScreenPoint;
        LOG("Did not resample, sending (%d, %d)\n", singleTouch.x, singleTouch.y);
      } else {
        ResampleTouchMoves(touchMove, aFirstTouch, aSecondTouch, aVsyncTime);
      }
    }
  }

  DispatchTouchEvent(touchMove);
}

static int
Interpolate(int start, int end, TimeDuration aFrameDiff, TimeDuration aTouchDiff)
{
  return start + (((end - start) * aFrameDiff.ToMicroseconds()) / aTouchDiff.ToMicroseconds());
}

static const SingleTouchData&
GetTouchByID(const SingleTouchData& aCurrentTouch, MultiTouchInput& aOtherTouch)
{
  int32_t id = aCurrentTouch.mIdentifier;
  for (size_t i = 0; i < aOtherTouch.mTouches.Length(); i++) {
    SingleTouchData& touch = aOtherTouch.mTouches[i];
    if (touch.mIdentifier == id) {
      return touch;
    }
  }

  // We can have situations where a previous touch event had 2 fingers
  // and we lift 1 finger off. In those cases, we won't find the touch event
  // with given id, so just return the current touch, which will be resampled
  // without modification and dispatched.
  return aCurrentTouch;
}

static void
ResampleTouch(MultiTouchInput& aOutTouch, MultiTouchInput& aCurrent,
              MultiTouchInput& aOther, TimeDuration aFrameDiff,
              TimeDuration aTouchDiff, bool aInterpolate)
{
  aOutTouch = aCurrent;

  // Make sure we only resample the correct finger.
  for (size_t i = 0; i < aOutTouch.mTouches.Length(); i++) {
    const SingleTouchData& current = aCurrent.mTouches[i];
    const SingleTouchData& other = GetTouchByID(current, aOther);

    const ScreenIntPoint& currentTouchPoint = current.mScreenPoint;
    const ScreenIntPoint& otherTouchPoint = other.mScreenPoint;

    ScreenIntPoint newSamplePoint;
    newSamplePoint.x = Interpolate(currentTouchPoint.x, otherTouchPoint.x, aFrameDiff, aTouchDiff);
    newSamplePoint.y = Interpolate(currentTouchPoint.y, otherTouchPoint.y, aFrameDiff, aTouchDiff);

    aOutTouch.mTouches[i].mScreenPoint = newSamplePoint;

    const char* type = "extrapolate";
    if (aInterpolate) {
      type = "interpolate";
    }

    float alpha = aFrameDiff.ToMicroseconds() / aTouchDiff.ToMicroseconds();
    LOG("%s current (%d, %d), other (%d, %d) to (%d, %d) alpha %f, touch diff %f, frame diff %f\n",
        type,
        currentTouchPoint.x, currentTouchPoint.y,
        otherTouchPoint.x, otherTouchPoint.y,
        newSamplePoint.x, newSamplePoint.y,
        alpha, aTouchDiff.ToMilliseconds(), aFrameDiff.ToMilliseconds());
  }
}

// Interpolates with the touch event prior to SampleTime
// and with the future touch event past sample time
TimeDuration
GeckoTouchDispatcher::InterpolateTouch(MultiTouchInput& aOutTouch,
                                       MultiTouchInput& aFutureTouch,
                                       MultiTouchInput& aCurrentTouch,
                                       TimeStamp aSampleTime)
{
  // currentTouch < SampleTime < futureTouch
  TimeDuration touchTimeDiff = aFutureTouch.mTimeStamp - aCurrentTouch.mTimeStamp;
  TimeDuration frameDiff = aSampleTime - aCurrentTouch.mTimeStamp;
  ResampleTouch(aOutTouch, aCurrentTouch, aFutureTouch, frameDiff, touchTimeDiff, true);
  return frameDiff;
}

// Extrapolates from the previous two touch events before sample time
// and extrapolates them to sample time.
TimeDuration
GeckoTouchDispatcher::ExtrapolateTouch(MultiTouchInput& aOutTouch,
                                       MultiTouchInput& aCurrentTouch,
                                       MultiTouchInput& aPrevTouch,
                                       TimeStamp aSampleTime)
{
  // prevTouch < currentTouch < SampleTime
  TimeDuration touchTimeDiff = aCurrentTouch.mTimeStamp - aPrevTouch.mTimeStamp;
  TimeDuration maxResampleTime = TimeDuration::FromMicroseconds(
                                 std::min(touchTimeDiff.ToMicroseconds() / 2,
                                           mMaxPredict.ToMicroseconds()));
  TimeStamp maxTimestamp = aCurrentTouch.mTimeStamp + maxResampleTime;

  if (aSampleTime > maxTimestamp) {
    aSampleTime = maxTimestamp;
    LOG("Overshot extrapolation time, adjusting sample time\n");
  }

  // This has to be signed int since it is negative
  TimeDuration frameDiff = aCurrentTouch.mTimeStamp - aSampleTime;
  ResampleTouch(aOutTouch, aCurrentTouch, aPrevTouch, frameDiff, touchTimeDiff, false);
  return frameDiff;
}

// Get the last 2 touch events before the aVsyncTime
// Returns true if we found two valid touch events
bool
GeckoTouchDispatcher::GetTouchEvents(MultiTouchInput& aFirstTouch,
                                     MultiTouchInput& aSecondTouch,
                                     TimeStamp aVsyncTime)
{
  if (!mResamplingEnabled || mTouchMoveEvents.size() == 1) {
    aFirstTouch = mTouchMoveEvents.back();
    return false;
  }

  int i = 0;
  for (i = 0; i < mTouchMoveEvents.size(); i++) {
    MultiTouchInput& aTouch = mTouchMoveEvents[i];
    if (aTouch.mTimeStamp > aVsyncTime) {
      i--;
      break;
    }
  }

  if (i == mTouchMoveEvents.size()) {
    i--;
  }

  // Can have situations where this vsync message is really late on the main
  // thread so every touch occured after this vsync. This usually happens after
  // a long delay where we already cleared out all the touch events. Just send
  // the last touch event and hope to catch up
  if (i <= 0) {
    aFirstTouch = mTouchMoveEvents[0];
    return false;
  }

  MOZ_RELEASE_ASSERT(i > 0);
  aFirstTouch = mTouchMoveEvents[i];
  aSecondTouch = mTouchMoveEvents[i-1];
  mTouchMoveEvents.erase(mTouchMoveEvents.begin(),
                         mTouchMoveEvents.begin() + i);
  return true;
}

// aSecondTouch < aFirstTouch < aVsyncTime
void
GeckoTouchDispatcher::ResampleTouchMoves(MultiTouchInput& aOutTouch,
                                         MultiTouchInput& aFirstTouch,
                                         MultiTouchInput& aSecondTouch,
                                         TimeStamp aVsyncTime)
{
  TimeStamp sampleTime = aVsyncTime - mVsyncAdjust;
  TimeDuration touchTimeAdjust = 0;

  if (aFirstTouch.mTimeStamp > sampleTime) {
    touchTimeAdjust = InterpolateTouch(aOutTouch, aFirstTouch, aSecondTouch, sampleTime);
    aOutTouch.mTime = aSecondTouch.mTime + touchTimeAdjust.ToMilliseconds();
  } else {
    touchTimeAdjust = ExtrapolateTouch(aOutTouch, aFirstTouch, aSecondTouch, sampleTime);
    aOutTouch.mTime = aFirstTouch.mTime - touchTimeAdjust.ToMilliseconds();
  }

  aOutTouch.mTimeStamp = sampleTime;
}

// Some touch events get sent as mouse events. If APZ doesn't capture the event
// and if a touch only has 1 touch input, we can send a mouse event.
void
GeckoTouchDispatcher::DispatchMouseEvent(MultiTouchInput& aMultiTouch,
                                         bool aForwardToChildren)
{
  WidgetMouseEvent mouseEvent = ToWidgetMouseEvent(aMultiTouch, nullptr);
  if (mouseEvent.message == NS_EVENT_NULL) {
    return;
  }

  mouseEvent.mFlags.mNoCrossProcessBoundaryForwarding = !aForwardToChildren;
  nsWindow::DispatchInputEvent(mouseEvent);
}

static bool
IsExpired(const MultiTouchInput& aTouch)
{
  // No pending events, the filter state can be updated.
  uint64_t timeNowMs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000000;
  return (timeNowMs - aTouch.mTime) > kInputExpirationThresholdMs;
}
void
GeckoTouchDispatcher::DispatchTouchEvent(MultiTouchInput& aMultiTouch)
{
  if ((aMultiTouch.mType == MultiTouchInput::MULTITOUCH_END ||
       aMultiTouch.mType == MultiTouchInput::MULTITOUCH_CANCEL) &&
      aMultiTouch.mTouches.Length() == 1) {
    MutexAutoLock lock(mTouchQueueLock);
    mTouchMoveEvents.clear();
  } else if (aMultiTouch.mType == MultiTouchInput::MULTITOUCH_START &&
             aMultiTouch.mTouches.Length() == 1) {
    mTouchEventsFiltered = IsExpired(aMultiTouch);
  }

  if (mTouchEventsFiltered) {
    return;
  }

  bool captured = false;
  WidgetTouchEvent event = aMultiTouch.ToWidgetTouchEvent(nullptr);
  nsEventStatus status = nsWindow::DispatchInputEvent(event, &captured);

  if (mEnabledUniformityInfo && profiler_is_active()) {
    const char* touchAction = "Invalid";
    switch (aMultiTouch.mType) {
      case MultiTouchInput::MULTITOUCH_START:
        touchAction = "Touch_Event_Down";
        break;
      case MultiTouchInput::MULTITOUCH_MOVE:
        touchAction = "Touch_Event_Move";
        break;
      case MultiTouchInput::MULTITOUCH_END:
      case MultiTouchInput::MULTITOUCH_CANCEL:
        touchAction = "Touch_Event_Up";
        break;
    }

    const ScreenIntPoint& touchPoint = aMultiTouch.mTouches[0].mScreenPoint;
    TouchDataPayload* payload = new TouchDataPayload(touchPoint);
    PROFILER_MARKER_PAYLOAD(touchAction, payload);
  }

  if (!captured && (aMultiTouch.mTouches.Length() == 1)) {
    bool forwardToChildren = status != nsEventStatus_eConsumeNoDefault;
    DispatchMouseEvent(aMultiTouch, forwardToChildren);
  }
}

WidgetMouseEvent
GeckoTouchDispatcher::ToWidgetMouseEvent(const MultiTouchInput& aMultiTouch,
                                         nsIWidget* aWidget) const
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(),
                    "Can only convert To WidgetMouseEvent on main thread");

  uint32_t mouseEventType = NS_EVENT_NULL;
  switch (aMultiTouch.mType) {
    case MultiTouchInput::MULTITOUCH_START:
      mouseEventType = NS_MOUSE_BUTTON_DOWN;
      break;
    case MultiTouchInput::MULTITOUCH_MOVE:
      mouseEventType = NS_MOUSE_MOVE;
      break;
    case MultiTouchInput::MULTITOUCH_CANCEL:
    case MultiTouchInput::MULTITOUCH_END:
      mouseEventType = NS_MOUSE_BUTTON_UP;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Did not assign a type to WidgetMouseEvent");
      break;
  }

  WidgetMouseEvent event(true, mouseEventType, aWidget,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

  const SingleTouchData& firstTouch = aMultiTouch.mTouches[0];
  event.refPoint.x = firstTouch.mScreenPoint.x;
  event.refPoint.y = firstTouch.mScreenPoint.y;

  event.time = aMultiTouch.mTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  event.modifiers = aMultiTouch.modifiers;

  if (mouseEventType != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }

  return event;
}

} // namespace mozilla
