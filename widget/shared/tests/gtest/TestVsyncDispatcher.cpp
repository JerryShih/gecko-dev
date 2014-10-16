/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "nsThreadUtils.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Monitor.h"

// Test target
#include "VsyncDispatcher.h"
#include "VsyncDispatcherHostImpl.h"
#include "PlatformVsyncTimer.h"

using namespace mozilla;
using ::testing::NiceMock;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

// A manual timer for testing purepose.
// Send vsync notification while a test case explicit calls
// FakeTimer::SendVsync
class FakeTimer: public PlatformVsyncTimer
{
public:
  FakeTimer(VsyncTimerObserver* aObserver)
    : PlatformVsyncTimer(aObserver)
//    , mRate(60)
  {
    /* Pass */
  }

//  virtual uint32_t GetVsyncRate()
//  {
//    // Return 0 as a test?
//    // Discuss with jerry regarding to expected behavior
//    return mVsyncRate;
//  }

  // Send out a vsync notification manually.
  void SendVsync(double aMicroseconds)
  {
    TimeStamp ts = TimeStamp() + TimeDuration::FromMicroseconds(aMicroseconds);
    mObserver->NotifyVsync(ts);
  }

//  void SetVsyncRate(uint32_t aRate)
//  {
//    // Retrun an unreasonable value and define how VsyncTimerObserver
//    // handle this codition.
//    mRate = aRate;
//  }

  void AssignObserver(VsyncTimerObserver *aObserver)
  {
    mObserver = aObserver;
  }

protected:
//  uint32_t mRate;
};

class MockTimer: public FakeTimer
{
public:
  MockTimer()
    : FakeTimer(nullptr)
  {
    /* Pass */
  }

  void Init()
  {
    ON_CALL(*this, Startup())
      .WillByDefault(Return(true));
  }

  // Open question: do we need Startup/ Shutdown test cases?
  MOCK_METHOD0(Startup, bool());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(Enable, void(bool aEnable));
  MOCK_METHOD0(GetVsyncRate, uint32_t());
};

class MockObserver: public VsyncObserver
{
public:
  MOCK_METHOD2(VsyncTick, bool(TimeStamp aTimestamp, uint64_t aFrameNumber));
};

// Test fixture.
class SilkHostTest : public ::testing::Test
{
public:
  static PlatformVsyncTimer *Create(VsyncTimerObserver *aObserver)
  {
    gThis->mTimer.AssignObserver(aObserver);
    return &gThis->mTimer;
  }

  virtual void SetUp()
  {
    gThis = this;

    mTimer.Init();
    // Request: new PlatfomrVsyncTimerFactory API
    // We need to have a way to replace PlatformVsyncTimer by MockTimer.
    PlatformVsyncTimerFactory::SetCustomCreator(&SilkHostTest ::Create);

    mDispatcher = VsyncDispatcher::GetInstance();
    mHostImp = static_cast<VsyncDispatcherHostImpl *>(mDispatcher);

    mHostImp->Startup();
  }

  virtual void TearDown()
  {
    // XXX: Shutdonw lead crash check with Jerry
    //mHostImp->Shutdown();
    PlatformVsyncTimerFactory::SetCustomCreator(nullptr);
  }

protected:
  VsyncDispatcherHostImpl   *mHostImp;
  VsyncDispatcher           *mDispatcher;
  NiceMock<MockTimer>       mTimer;
  static SilkHostTest  *gThis;
};

/*static*/ SilkHostTest  *SilkHostTest::gThis;

//// Principle to check:
////   A user can get DispathcerHost interface from VsyncDispatcher in
////   chrome process.
////   A user can get DispatcherClient interface from VsyncDispatcher in
////   content process.
//TEST_F(SilkHostTest, QueryInterface)
//{
//  // Interface qurey test.
//  VsyncDispatcherClient* client = mDispatcher->AsVsyncDispatcherClient();
//  EXPECT_TRUE(client == nullptr);
//
//  VsyncDispatcherHost* host = mDispatcher->AsVsyncDispatcherHost();
//  EXPECT_TRUE(host != nullptr);
//}

// Principle to check:
//   A observer registers to via VsyncEventRegistry::AddObserver(_, true) should
//   keep receive vsync tick before unregistry.
TEST_F(SilkHostTest, AlwasyTriggerRegistry)
{
  NiceMock<MockObserver> observer;

  EXPECT_CALL(observer, VsyncTick(_,_))
    .Times(2)
    .WillOnce(Return(false))
    .WillOnce(Return(false));

  // Registration
  VsyncEventRegistry* registry = mDispatcher->GetInputDispatcherRegistry();
  registry->AddObserver(&observer, true);

  // Send twice, receive twice.
  mTimer.SendVsync(1000);
  mTimer.SendVsync(2000);

  // Unregistration.
  // After taht, send once, recieve none.
  registry->RemoveObserver(&observer, true);
  mTimer.SendVsync(3000);
}

// Principle to check:
//   A observer registers to via VsyncEventRegistry::AddObserver(_, false) should
//   receive one and only one vsync tick.
TEST_F(SilkHostTest, NotAlwasyTriggerRegistry)
{
  NiceMock<MockObserver> observer;

  EXPECT_CALL(observer, VsyncTick(_,_))
    .Times(1)
    .WillOnce(Return(false));

  // Registration
  VsyncEventRegistry* registry = mDispatcher->GetInputDispatcherRegistry();
  registry->AddObserver(&observer, false);

  // Send twice, receive once.
  mTimer.SendVsync(1000);
  mTimer.SendVsync(2000);

  registry->RemoveObserver(&observer, false);
}

// Priciple to check:
//   VsyncObserver should keepp the order of timestamp which comes from
//   PlatformTimer.
TEST_F(SilkHostTest, NotificationSequence)
{
  const int times = 10;

  NiceMock<MockObserver> observer;

  // Expect inputObserver recieve frame time from 0 to "times - 1"
  // in ascending assequence.
  {
    InSequence s;

    for (int i = 0; i < times; i++)
    {
      TimeStamp ts = TimeStamp() + TimeDuration::FromMicroseconds(i * 1000);
      EXPECT_CALL(observer, VsyncTick(ts,_))
        .WillOnce(Return(false))
        .RetiresOnSaturation();
    }
  }

  VsyncEventRegistry* registry = mDispatcher->GetCompositorRegistry();
  registry->AddObserver(&observer, true);

  for (int i = 0; i < times; i++)
  {
    mTimer.SendVsync(i * 1000);
  }

  registry->RemoveObserver(&observer, true);
}

// Principle to check:
//   VsyncDispatcher should enable vsync timer after the first observer been added
//   VsyncDispatcher should disable vsync timer after the lastest observer been removed
TEST_F(SilkHostTest, TimerEnabling)
{
  NiceMock<MockObserver> observer;
  {
    InSequence s;

    EXPECT_CALL(mTimer, Enable(true))
      .Times(1);
    EXPECT_CALL(mTimer, Enable(false))
      .Times(1);
  }

  // Add the first observer.
  VsyncEventRegistry* registry = mDispatcher->GetCompositorRegistry();
  registry->AddObserver(&observer, true);

  // Remove the latest observer.
  registry->RemoveObserver(&observer, true);
}

// Principle to check:
//   VsyncDispatcher should not alter time stamps passed from PlatfomrVsyncTimer
TEST_F(SilkHostTest, TimeStampPersistence)
{
  NiceMock<MockObserver> observer;

  TimeStamp ts = TimeStamp() + TimeDuration::FromMicroseconds(123000000);
  EXPECT_CALL(observer, VsyncTick(ts,_))
    .Times(1)
    .WillOnce(Return(false));

  VsyncEventRegistry* registry = mDispatcher->GetCompositorRegistry();
  registry->AddObserver(&observer, false);

  mTimer.SendVsync(123000000);

  registry->RemoveObserver(&observer, false);
}

// Principle to check
//   VsyncDispatcher should send vsync notifictions in the following order
//   1. InputDispatcher observers
//   2. Compositor observers
//   3. RefreshDriver observers.
TEST_F(SilkHostTest, ObserverPriority)
{
//  VsyncEventRegistry* refreshDriverRegistry = mDispatcher->GetRefreshDriverRegistry();
  VsyncEventRegistry* compositorRegistry = mDispatcher->GetCompositorRegistry();
  VsyncEventRegistry* inputDispatcherRegistry = mDispatcher->GetInputDispatcherRegistry();

  NiceMock<MockObserver> inputDispatcherObserver;
  NiceMock<MockObserver> compositorObserver;
//  NiceMock<MockObserver> refreshDriverobserver;

  // AddObserver in reverse order.
//  refreshDriverRegistry->AddObserver(&refreshDriverobserver, false);
  compositorRegistry ->AddObserver(&compositorObserver, false);
  inputDispatcherRegistry->AddObserver(&inputDispatcherObserver, false);

  {
    InSequence s;

    EXPECT_CALL(inputDispatcherObserver, VsyncTick(_,_))
      .Times(1)
      .WillOnce(Return(false));

    EXPECT_CALL(compositorObserver, VsyncTick(_,_))
      .Times(1)
      .WillOnce(Return(false));

//    EXPECT_CALL(refreshDriverobserver, VsyncTick(_,_))
//      .Times(1)
//      .WillOnce(Return(false));
  }

  mTimer.SendVsync(1000);
}


// Unclear test cases
// Jerry:
// What if a VyncTimerObserver take more then 16 ms in NotifyVsync callback?
// What's the behavior we expect in VsyncDispatcher?
// Write a perf log?
TEST_F(SilkHostTest, NotificationTimeout)
{

}
