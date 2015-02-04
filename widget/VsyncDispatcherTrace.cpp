/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcherTrace.h"
#include "base/time.h"
#include "nsDebug.h"

// Debug
#ifdef MOZ_WIDGET_GONK
#include "cutils/properties.h"
#endif

namespace mozilla {

VsyncLatencyLogger::LoggerMap VsyncLatencyLogger::mLoggerMap;

int
GetVsyncDispatcherPropValue(const char *aPropName, int aDefaultValue)
{
  if (!aPropName) {
    return aDefaultValue;
  }

  int intValue = aDefaultValue;

#ifdef MOZ_WIDGET_GONK
  char propValueString[PROPERTY_VALUE_MAX];

  if (property_get(aPropName, propValueString, "") > 0) {
    intValue = atoi(propValueString);
  }
#endif

  return intValue;
}

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
    if (GetVsyncDispatcherPropValue("silk.timer.log.raw", 0) == 0) {
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
                aDataValue * 0.001f);
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
                aAvg * 0.001f,
                aStd * 0.001f,
                aStd / aAvg * 100.0f,
                aMaxFrameNumber,
                aMaxDataValue * 0.001f,
                aMinFrameNumber,
                aMinDataValue * 0.001f);
}

} //mozilla
