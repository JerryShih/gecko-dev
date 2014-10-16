/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherTrace.h"
#include "base/time.h"
#include "nsDebug.h"

// Debug
#include "cutils/properties.h"

namespace mozilla {

VsyncLatencyLogger::LoggerMap VsyncLatencyLogger::mLoggerMap;

/*static*/ VsyncLatencyLogger*
VsyncLatencyLogger::CreateLogger(const char* aName)
{
  VsyncLatencyLogger* logger = nullptr;
  LoggerMap::iterator iterator = mLoggerMap.find(aName);

  if (iterator == mLoggerMap.end()) {
    iterator = mLoggerMap.insert(std::make_pair(aName, VsyncLatencyLogger(aName))).first;
    logger = &(iterator->second);
  }
  else {
    logger = &(iterator->second);
  }

  return logger;
}

VsyncLatencyLogger::VsyncLatencyLogger(const char* aName)
  : mStatistician(aName, &PrintRawCallback, &PrintStatisticCallback)
{

}

VsyncLatencyLogger::~VsyncLatencyLogger()
{

}

void
VsyncLatencyLogger::Start(uint32_t aFrameNum)
{
  mPendingData[aFrameNum] = base::TimeTicks::HighResNow().ToInternalValue();
}

void
VsyncLatencyLogger::End(uint32_t aFrameNum)
{
  PendingDataMap::iterator iterator = mPendingData.find(aFrameNum);

  mStatistician.Update(aFrameNum,
                       base::TimeTicks::HighResNow().ToInternalValue() - iterator->second);

  mPendingData.erase(iterator);
}

void
VsyncLatencyLogger::Update(uint32_t aFrameNum, int64_t aLatencyUS)
{
  mStatistician.Update(aFrameNum, aLatencyUS);
}

void
VsyncLatencyLogger::PrintRaw()
{
  mStatistician.PrintRawData();
}

void
VsyncLatencyLogger::PrintStatistic()
{
  mStatistician.PrintStatisticData();
}

void
VsyncLatencyLogger::Reset()
{
  mStatistician.Reset();
}

bool
VsyncLatencyLogger::Flush(uint32_t aFrameNum)
{
  bool ret = false;
  const int n = 256; // magic number /o/
  if(!(aFrameNum % n)){
    char propValue[PROPERTY_VALUE_MAX];
    property_get("silk.timer.log.raw", propValue, "0");
    if (atoi(propValue) == 0) {
      PrintStatistic();
    } else {
      PrintRaw();
    }
    Reset();
    ret = true;
  }
  return ret;
}

void
VsyncLatencyLogger::PrintRawCallback(const char *aMsg, uint32_t aFrameNumber, int64_t aDataValue)
{
  printf_stderr("%-20s, %d, %5.3fms",
                aMsg,
                aFrameNumber,
                aDataValue * 1.0f);
}

void
VsyncLatencyLogger::PrintStatisticCallback(const char *aMsg,
                                           float aAvg,
                                           float aStd,
                                           uint32_t aMaxFrameNumber,
                                           int64_t aMaxDataValue,
                                           uint32_t aMinFrameNumber,
                                           int64_t aMinDataValue)
{
  printf_stderr("%-20s, avg:%5.3fms, std:%5.3fms, cv:%5.3f%% max:(%d, %5.3fms), min:(%d, %5.3fms)",
                aMsg,
                aAvg,
                aStd,
                aStd / aAvg * 100.0f,
                aMaxFrameNumber,
                aMaxDataValue * 1.0f,
                aMinFrameNumber,
                aMinDataValue * 1.0f);
}

} //mozilla
