/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_shared_VsyncDispatcherHelper_h
#define mozilla_widget_shared_VsyncDispatcherHelper_h

#include "base/task.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

class VsyncRegistryNonThreadSafePolicyImpl
{
public:
  VsyncRegistryNonThreadSafePolicyImpl(Mutex&) { }
};

// The thread policy for vsync event registry.
typedef MutexAutoLock VsyncRegistryThreadSafePolicy;
typedef VsyncRegistryNonThreadSafePolicyImpl VsyncRegistryNonThreadSafePolicy;

// This class provide the simple registering implementation for vsync observer.
// We can set different thread policy to protect the observer list when we
// access. We use VsyncRegistryThreadSafePolicy by default. Please check
// VsyncRegistryThreadSafePolicy and VsyncRegistryNonThreadSafePolicy.
template<typename ThreadPolicy = VsyncRegistryThreadSafePolicy>
class VsyncEventRegistryImpl : public VsyncEventRegistry
{
protected:
  typedef nsTArray<VsyncObserver*> ObserverList;
  typedef ThreadPolicy ThreadGuard;

public:
  VsyncEventRegistryImpl(VsyncDispatcher* aVsyncDispatcher)
    : mVsyncDispatcher(aVsyncDispatcher)
    , mObserverMutex("VsyncEventRegistry Lock")
    , mVsyncEventNeeded(false)
  {
    MOZ_ASSERT(mVsyncDispatcher);
  }

  virtual ~VsyncEventRegistryImpl() { }

  // Add/Remove vsync observer.
  virtual void AddObserver(VsyncObserver* aVsyncObserver,
                           bool aAlwaysTrigger) MOZ_OVERRIDE
  {
    ThreadGuard threadGuard(mObserverMutex);

    ObserverList* list = GetObserverList(aAlwaysTrigger);
    if (!list->Contains(aVsyncObserver)) {
      list->AppendElement(aVsyncObserver);
      CheckObserverNumber();
    }
  }

  virtual void RemoveObserver(VsyncObserver* aVsyncObserver,
                              bool aAlwaysTrigger) MOZ_OVERRIDE
  {
    ThreadGuard threadGuard(mObserverMutex);

    ObserverList* list = GetObserverList(aAlwaysTrigger);
    if (list->Contains(aVsyncObserver)) {
      list->RemoveElement(aVsyncObserver);
      CheckObserverNumber();
    }
  }

  // Dispatch vsync event to all registered observer.
  virtual bool Dispatch(TimeStamp aTimestamp, uint64_t aFrameNumber) MOZ_OVERRIDE
  {
    ThreadGuard threadGuard(mObserverMutex);

    for (ObserverList::size_type i = 0; i < mObserverList.Length(); ++i) {
      mObserverList[i]->VsyncTick(aTimestamp, aFrameNumber);
    }

    for (ObserverList::size_type i = 0; i < mTemporaryObserverList.Length(); ++i) {
      mTemporaryObserverList[i]->VsyncTick(aTimestamp, aFrameNumber);
    }
    mTemporaryObserverList.Clear();

    return (GetObserverNum() > 0);
  }

protected:
  ObserverList* GetObserverList(bool aAlwaysTrigger)
  {
    return aAlwaysTrigger ? &mObserverList : &mTemporaryObserverList;
  }

  // Return the total observer number.
  virtual uint32_t GetObserverNum() const
  {
    return mObserverList.Length() + mTemporaryObserverList.Length();
  }

  void CheckObserverNumber()
  {
    if (!!GetObserverNum() !=  mVsyncEventNeeded) {
      mVsyncEventNeeded = !mVsyncEventNeeded;

      if (mVsyncEventNeeded) {
        mVsyncDispatcher->VsyncTickNeeded();
      }
    }
  }

  VsyncDispatcher* mVsyncDispatcher;

  mutable Mutex mObserverMutex;

  bool mVsyncEventNeeded;

  // The observer in mObserverList will always retain until we call remove
  // function.
  ObserverList mObserverList;
  // Unlike mObserverList, the mTemporaryObserverList will be cleared after
  // tick.
  ObserverList mTemporaryObserverList;
};

// The delegate helper class for nsThread task.
template<typename T, typename Method, typename Params>
class NSVsyncRunnableMethod : public nsCancelableRunnable
{
public:
  NSVsyncRunnableMethod(T* aObj, Method aMeth, const Params& aParams)
    : mObj(aObj)
    , mMethod(aMeth)
    , mParams(aParams)
  {
    if (aObj) {
      mObj->AddRef();
    }
  }

  ~NSVsyncRunnableMethod()
  {
    ReleaseCallee();
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    if (mObj) {
      DispatchToMethod(mObj, mMethod, mParams);
    }

    return NS_OK;
  }

  NS_IMETHOD Cancel() MOZ_OVERRIDE
  {
    ReleaseCallee();

    return NS_OK;
  }

private:
  void ReleaseCallee() {
    if (mObj) {
      mObj->Release();
      mObj = nullptr;
    }
  }

  T* mObj;
  Method mMethod;
  Params mParams;
};

template<typename T, typename Method>
inline nsCancelableRunnable*
NewNSVsyncRunnableMethod(T* object, Method method)
{
  return new NSVsyncRunnableMethod<T, Method, Tuple0>(object,
                                                      method,
                                                      MakeTuple());
}

template<typename T, typename Method, typename A>
inline nsCancelableRunnable*
NewNSVsyncRunnableMethod(T* object, Method method, const A& a)
{
  return new NSVsyncRunnableMethod<T, Method, Tuple1<A> >(object,
                                                          method,
                                                          MakeTuple(a));
}

template<typename T, typename Method, typename A, typename B>
inline nsCancelableRunnable*
NewNSVsyncRunnableMethod(T* object, Method method, const A& a, const B& b)
{
  return new NSVsyncRunnableMethod<T, Method, Tuple2<A, B> >(object,
                                                             method,
                                                             MakeTuple(a, b));
}

template<typename T, typename Method, typename A, typename B, typename C>
inline nsCancelableRunnable*
NewNSVsyncRunnableMethod(T* object, Method method, const A& a, const B& b, const C& c)
{
  return new NSVsyncRunnableMethod<T, Method, Tuple3<A, B, C> >(object,
                                                                method,
                                                                MakeTuple(a, b, c));
}

template<typename T, typename Method, typename A, typename B, typename C, typename D>
inline nsCancelableRunnable*
NewNSVsyncRunnableMethod(T* object, Method method, const A& a, const B& b, const C& c, const D& d)
{
  return new NSVsyncRunnableMethod<T, Method, Tuple4<A, B, C, D> >(object,
                                                                   method,
                                                                   MakeTuple(a, b, c, d));
}

template<typename T, typename Method, typename A, typename B, typename C, typename D, typename E>
inline nsCancelableRunnable*
NewNSVsyncRunnableMethod(T* object, Method method, const A& a, const B& b, const C& c, const D& d, const E& e)
{
  return new NSVsyncRunnableMethod<T, Method, Tuple5<A, B, C, D, E> >(object,
                                                                      method,
                                                                      MakeTuple(a, b, c, d, e));
}

} // namespace mozilla

#endif // mozilla_widget_shared_VsyncDispatcherHelper_h
