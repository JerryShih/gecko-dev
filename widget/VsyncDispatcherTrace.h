/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_VsyncDispatcherTrace_h
#define mozilla_widget_VsyncDispatcherTrace_h

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <cmath>
#include "nsDebug.h"

#ifdef MOZ_WIDGET_GONK
#define SYSTRACE_LABEL
#ifdef SYSTRACE_LABEL
# define ATRACE_TAG ATRACE_TAG_ALWAYS
// We need HAVE_ANDROID_OS to be defined for Trace.h.
// If its not set we will set it temporary and remove it.
# ifndef HAVE_ANDROID_OS
#   define HAVE_ANDROID_OS
#   define REMOVE_HAVE_ANDROID_OS
# endif
// Android source code will include <cutils/trace.h> before this. There is no
// HAVE_ANDROID_OS defined in Firefox OS build at that time. Enabled it globally
// will cause other build break. So atrace_begin and atrace_end are not defined.
// It will cause a build-break when we include <utils/Trace.h>. Use undef
// _LIBS_CUTILS_TRACE_H will force <cutils/trace.h> to define atrace_begin and
// atrace_end with defined HAVE_ANDROID_OS again. Then there is no build-break.
# undef _LIBS_CUTILS_TRACE_H
# include <utils/Trace.h>

# define VSYNC_TRACE_OBJ_NAME_PASTE(name, line) name ## line
# define VSYNC_TRACE_OBJ_NAME_EXPEND(name, line) VSYNC_TRACE_OBJ_NAME_PASTE(name, line)
# define VSYNC_TRACE_OBJ_NAME(name) VSYNC_TRACE_OBJ_NAME_EXPEND(name, __LINE__)
# define VSYNC_SCOPED_SYSTRACE_LABEL(name) ATRACE_NAME(name)
# define VSYNC_SCOPED_SYSTRACE_LABEL_PRINTF(name, ...) VsyncScopedTrace VSYNC_TRACE_OBJ_NAME(scopeTraceObj)(name, ##__VA_ARGS__)
# define VSYNC_SYSTRACE_LABEL_BEGIN(name) ATRACE_BEGIN(name)
# define VSYNC_SYSTRACE_LABEL_END() ATRACE_END()
# define VSYNC_SYSTRACE_LABEL_BEGIN_PRINTF(name, ...) VsyncTraceLabel VSYNC_TRACE_OBJ_NAME(labelObj)(name, ##__VA_ARGS__)
# define VSYNC_SYSTRACE_LABEL_END_PRINTF() ATRACE_END()
# define VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN(id, name) ATRACE_ASYNC_BEGIN(name, id)
# define VSYNC_ASYNC_SYSTRACE_LABEL_END(id, name) ATRACE_ASYNC_END(name, id)
# define VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN_PRINTF(id, name, ...) VsyncAsyncTraceLabelBegin VSYNC_TRACE_OBJ_NAME(asyncLabelObj)(id, name, ##__VA_ARGS__)
# define VSYNC_ASYNC_SYSTRACE_LABEL_END_PRINTF(id, name, ...) VsyncAsyncTraceLabelEnd VSYNC_TRACE_OBJ_NAME(asyncLabelObj)(id, name, ##__VA_ARGS__)

namespace mozilla {

class VsyncScopedTrace
{
public:
  VsyncScopedTrace(const char* name, ...)
  {
#ifdef HAVE_ANDROID_OS
    va_list args;
    char str[128];
    va_start(args, name);
    vsnprintf(str, 128, name, args);
    va_end(args);
    atrace_begin(ATRACE_TAG, str);
#endif
  }

  ~VsyncScopedTrace()
  {
#ifdef HAVE_ANDROID_OS
    atrace_end (ATRACE_TAG);
#endif
  }
};

class VsyncTraceLabel
{
public:
  VsyncTraceLabel(const char* name, ...)
  {
#ifdef HAVE_ANDROID_OS
    va_list args;
    char str[128];
    va_start(args, name);
    vsnprintf(str, 128, name, args);
    va_end(args);
    atrace_begin(ATRACE_TAG, str);
#endif
  }
};

class VsyncAsyncTraceLabelBegin
{
public:
  VsyncAsyncTraceLabelBegin(int32_t id, const char* name, ...)
  {
#ifdef HAVE_ANDROID_OS
    va_list args;
    char str[128];
    va_start(args, name);
    vsnprintf(str, 128, name, args);
    va_end(args);
    atrace_async_begin(ATRACE_TAG, str, id);
#endif
  }
};

class VsyncAsyncTraceLabelEnd
{
public:
  VsyncAsyncTraceLabelEnd(int32_t id, const char* name, ...)
  {
#ifdef HAVE_ANDROID_OS
    va_list args;
    char str[128];
    va_start(args, name);
    vsnprintf(str, 128, name, args);
    va_end(args);
    atrace_async_end(ATRACE_TAG, str, id);
#endif
  }
};

} // mozilla

# ifdef REMOVE_HAVE_ANDROID_OS
#  undef HAVE_ANDROID_OS
#  undef REMOVE_HAVE_ANDROID_OS
# endif

#endif //SYSTRACE_LABEL

#else

# define VSYNC_SCOPED_SYSTRACE_LABEL(name)
# define VSYNC_SCOPED_SYSTRACE_LABEL_PRINTF(name, ...)
# define VSYNC_SYSTRACE_LABEL_BEGIN(name)
# define VSYNC_SYSTRACE_LABEL_END()
# define VSYNC_SYSTRACE_LABEL_BEGIN_PRINTF(name, ...)
# define VSYNC_SYSTRACE_LABEL_END_PRINTF()
# define VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN(id, name)
# define VSYNC_ASYNC_SYSTRACE_LABEL_END(id, name)
# define VSYNC_ASYNC_SYSTRACE_LABEL_BEGIN_PRINTF(id, name, ...)
# define VSYNC_ASYNC_SYSTRACE_LABEL_END_PRINTF(id, name, ...)

#endif  //MOZ_WIDGET_GONK

namespace mozilla {

// Get the setting value of the vsync dispatcher specific prop name.
// Return aDefaultValue if the prop name is unset.
int
GetVsyncDispatcherPropValue(const char *aPropName, int aDefaultValue = 0);

template<typename DataType, uint32_t DataNum, bool aCircularRecording = true>
class DataStatistician
{
public:
  typedef void (*PrintRawFunc)(const char* aMsg, uint32_t aFrameNumber, DataType aDataValue);
  typedef void (*PrintStatisticDataFunc)(const char* aMsg,
                                         float aAvg,
                                         float aStd,
                                         uint32_t aMaxFrameNumber,
                                         DataType aMaxDataValue,
                                         uint32_t aMinFrameNumber,
                                         DataType aMinDataValue);

  DataStatistician(const char* aMsg, PrintRawFunc aPrintRaw, PrintStatisticDataFunc aPrintStatistic)
    : mMsg(aMsg)
    , mDataArray(DataNum)
    , mPrintRawFunc(aPrintRaw)
    , mPrintStatisticFunc(aPrintStatistic)
  {
    Reset();
  }

  ~DataStatistician()
  {

  }

  void Update(uint32_t aFrameNumber, DataType aData, bool aCorner = false)
  {
    if (!aCircularRecording) {
      if ( mNum >= DataNum) {
        return;
      }
    }

    mDataArray[mNextDataIndex] = LogData(aData, aFrameNumber, aCorner);
    if (aCorner) {
      if (mNextDataIndex > 0) {
        mDataArray[mNextDataIndex-1].SetCorner();
      } else if (!mDataArray.empty()) {
        // special, if mNextDataIndex == 0
        mDataArray[mDataArray.size()-1].SetCorner();
      }
    }
    mNextDataIndex = (mNextDataIndex + 1) % DataNum;
    mNum = std::min(mNum+1, DataNum);
  }

  void Reset()
  {
    mNum = 0;
    mNextDataIndex = 0;
  }

  void PrintRawData()
  {
    if (!mNum) {
      return;
    }

    uint32_t index = GetStartDataIndex();
    for (uint32_t i = 0; i < mNum; ++i) {
      if (mPrintRawFunc) {
        mPrintRawFunc(mMsg.c_str(), mDataArray[index].mFrameNumber, mDataArray[index].mDataValue);
      }
      else {
        PrintRawDefault(mMsg.c_str(), mDataArray[index].mFrameNumber, mDataArray[index].mDataValue, mDataArray[index].mCorner);
      }
      index = (index + 1) % DataNum;
    }
  }

  void PrintStatisticData()
  {
    if (!mNum) {
      return;
    }

    uint32_t maxIndex = GetMaxIndex();
    uint32_t minIndex = GetMinIndex();

    if (mPrintStatisticFunc) {
      mPrintStatisticFunc(mMsg.c_str(),
                          GetAVG(),
                          GetSTD(),
                          mDataArray[maxIndex].mFrameNumber,
                          mDataArray[maxIndex].mDataValue,
                          mDataArray[minIndex].mFrameNumber,
                          mDataArray[minIndex].mDataValue);
    }
    else {
      PrintStatisticDefault(mMsg.c_str(),
                            GetAVG(),
                            GetSTD(),
                            mDataArray[maxIndex].mFrameNumber,
                            mDataArray[maxIndex].mDataValue,
                            mDataArray[minIndex].mFrameNumber,
                            mDataArray[minIndex].mDataValue);
    }
  }

private:
  class LogData
  {
  public:
    LogData()
      : mDataValue(0)
      , mFrameNumber(0)
      , mCorner(false)
    {
    }

    LogData(DataType aDataValue, uint32_t aFrameNumber, bool aCorner = false)
      : mDataValue(aDataValue)
      , mFrameNumber(aFrameNumber)
      , mCorner(aCorner)
    {
    }

    LogData(const LogData& aLogData)
      : mDataValue(aLogData.mDataValue)
      , mFrameNumber(aLogData.mFrameNumber)
      , mCorner(aLogData.mCorner)
    {
    }

    LogData& operator=(const LogData& rLogData)
    {
      mDataValue = rLogData.mDataValue;
      mFrameNumber = rLogData.mFrameNumber;
      mCorner = rLogData.mCorner;

      return *this;
    }

    void SetCorner(bool aSet = true)
    {
      mCorner = aSet;
    }

    DataType mDataValue;
    uint32_t mFrameNumber;
    bool mCorner;
  };

  void PrintRawDefault(const char* aMsg, uint32_t aFrameNumber, DataType aDataValue, bool aCorner)
  {
    if (aCorner) {
      return;
    }

    printf_stderr("%-20s, %d, %5.3f%s",
                  aMsg,
                  aFrameNumber,
                  (float)aDataValue);
  }
  void PrintStatisticDefault(const char* aMsg,
                             float aAvg,
                             float aStd,
                             uint32_t aMaxFrameNumber,
                             int64_t aMaxDataValue,
                             uint32_t aMinFrameNumber,
                             int64_t aMinDataValue)
  {
    printf_stderr("%-20s, avg:%5.3f, std:%5.3f, cv:%5.3f%% max:(%d, %5.3f), min:(%d, %5.3f)",
                  aMsg,
                  aAvg * 0.001f,
                  aStd * 0.001f,
                  aStd / aAvg * 100.0f,
                  aMaxFrameNumber,
                  aMaxDataValue * 0.001f,
                  aMinFrameNumber,
                  aMinDataValue * 0.001f);
  }

  uint32_t GetStartDataIndex()
  {
    return (DataNum - mNum + mNextDataIndex) % DataNum;
  }

  uint32_t GetMaxIndex()
  {
    uint32_t maxValue = std::numeric_limits<DataType>::min();
    uint32_t maxIndex = 0;

    uint32_t index = GetStartDataIndex();
    for (uint32_t i = 0; i < mNum; ++i) {
      if (maxValue < mDataArray[index].mDataValue) {
        maxValue = mDataArray[index].mDataValue;
        maxIndex = index;
      }
      index = (index + 1) % DataNum;
    }

    return maxIndex;
  }

  uint32_t GetMinIndex()
  {
    uint32_t minValue = std::numeric_limits<DataType>::max();
    uint32_t minIndex = 0;

    uint32_t index = GetStartDataIndex();
    for (uint32_t i = 0; i < mNum; ++i) {
      if (minValue > mDataArray[index].mDataValue) {
        minValue = mDataArray[index].mDataValue;
        minIndex = index;
      }
      index = (index + 1) % DataNum;
    }

    return minIndex;
  }

  float GetAVG()
  {
    DataType total = 0;

    uint32_t index = GetStartDataIndex();
    for (uint32_t i = 0; i < mNum; ++i) {
      total += mDataArray[index].mDataValue;
      index = (index + 1) % DataNum;
    }

    return (float) total / mNum;
  }

  float GetSTD()
  {
    float variance = 0.0f;
    float avg = GetAVG();

    uint32_t index = GetStartDataIndex();
    for (uint32_t i = 0; i < mNum; ++i) {
      float delta = mDataArray[index].mDataValue - avg;
      variance += delta * delta;
      index = (index + 1) % DataNum;
    }

    return std::sqrt(variance / mNum);
  }

  std::string mMsg;

  std::vector<LogData> mDataArray;
  uint32_t mNum;
  uint32_t mNextDataIndex;

  PrintRawFunc mPrintRawFunc;
  PrintStatisticDataFunc mPrintStatisticFunc;
};

class VsyncLatencyLogger
{
public:
  static VsyncLatencyLogger* CreateLogger(const char* aName);

  VsyncLatencyLogger(const char* aName);
  ~VsyncLatencyLogger();

  // Call Start/End for the interested module latency.
  void Start(uint32_t aFrameNum);
  void End(uint32_t aFrameNum);

  // Just log the latency value.
  void Update(uint32_t aFrameNum, int64_t aLatencyUS);

  void PrintRaw();
  void PrintStatistic();
  void Reset();
  bool Flush(uint32_t aFrameNum);

private:
  static void PrintRawCallback(const char* aMsg, uint32_t aFrameNumber, int64_t aDataValue);
  static void PrintStatisticCallback(const char* aMsg,
                                     float aAvg,
                                     float aStd,
                                     uint32_t aMaxFrameNumber,
                                     int64_t aMaxDataValue,
                                     uint32_t aMinFrameNumber,
                                     int64_t aMinDataValue);

  DataStatistician<int64_t, 256, false> mStatistician;

  typedef std::map<uint32_t, int64_t> PendingDataMap;
  PendingDataMap mPendingData;

  typedef std::map<std::string, VsyncLatencyLogger> LoggerMap;
  static LoggerMap mLoggerMap;
};

} //mozilla

#endif  //mozilla_widget_VsyncDispatcherTrace_h
