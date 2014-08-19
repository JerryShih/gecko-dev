/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VsyncDispatcherHelper_h
#define mozilla_VsyncDispatcherHelper_h

#include "base/task.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"

namespace mozilla {

// This is the VsyncDispatcher observer list helper class.
// After adding/removing, we will check the observer number. If there is no
// observer in VsyncDispatcher, we can disable/enable the ipc vsync event
// notification in need.
class ObserverListHelper
{
public:
  template <typename DispatcherType, typename Type>
  static void Add(DispatcherType* aDispatcher, nsTArray<Type*>* aList, Type* aItem)
  {
    if (!aList->Contains(aItem)) {
      aList->AppendElement(aItem);
    }

    aDispatcher->EnableVsyncNotificationIfhasObserver();
  }

  template <typename DispatcherType, typename Type>
  static void Remove(DispatcherType* aDispatcher, nsTArray<Type*>* aList, Type* aItem)
  {
    typedef nsTArray<Type*> ArrayType;
    typename ArrayType::index_type index = aList->IndexOf(aItem);

    if (index != ArrayType::NoIndex) {
      aList->RemoveElementAt(index);
    }

    aDispatcher->EnableVsyncNotificationIfhasObserver();
  }

  template <typename DispatcherType, typename Type>
  static void AsyncAdd(DispatcherType* aDispatcher,
                      nsTArray<Type*>* aList,
                      Type* aItem)
  {
    aDispatcher->GetMessageLoop()->PostTask(FROM_HERE,
                                            NewRunnableFunction(&ObserverListHelper::Add<DispatcherType, Type>,
                                            aDispatcher,
                                            aList,
                                            aItem));
  }

  template <typename DispatcherType, typename Type>
  static void AsyncRemove(DispatcherType* aDispatcher,
                         nsTArray<Type*>* aList,
                         Type* aItem)
  {
    aDispatcher->GetMessageLoop()->PostTask(FROM_HERE,
                                            NewRunnableFunction(&ObserverListHelper::Remove<DispatcherType, Type>,
                                            aDispatcher,
                                            aList,
                                            aItem));
  }

  template <typename DispatcherType, typename Type>
  static void WaitForAsyncAdd(const char* aTaskName,
                              DispatcherType* aDispatcher,
                              nsTArray<Type*>* aList,
                              Type* aItem)
  {
    // Deadlock.
    MOZ_ASSERT(!aDispatcher->IsInVsyncDispatcherHostThread());

    Monitor monitor(aTaskName);
    MonitorAutoLock lock(monitor);
    bool done = false;

    aDispatcher->GetMessageLoop()->PostTask(FROM_HERE,
                                            NewRunnableFunction(&ObserverListHelper::AddWithNotify<DispatcherType, Type>,
                                            aDispatcher,
                                            aList,
                                            aItem,
                                            &monitor,
                                            &done));
    lock.Wait(PR_MillisecondsToInterval(32));
    if (!done) {
      printf_stderr("Wait task timeout: %s", aTaskName);
    }
  }

  template <typename DispatcherType, typename Type>
  static void WaitForAsyncRemove(const char* aTaskName,
                                 DispatcherType* aDispatcher,
                                 nsTArray<Type*>* aList,
                                 Type* aItem)
  {
    // Deadlock.
    MOZ_ASSERT(!aDispatcher->IsInVsyncDispatcherHostThread());

    Monitor monitor(aTaskName);
    MonitorAutoLock lock(monitor);
    bool done = false;

    aDispatcher->GetMessageLoop()->PostTask(FROM_HERE,
                                            NewRunnableFunction(&ObserverListHelper::RemoveWithNotify<DispatcherType, Type>,
                                            aDispatcher,
                                            aList,
                                            aItem,
                                            &monitor,
                                            &done));
    lock.Wait(PR_MillisecondsToInterval(32));
    if (!done) {
      printf_stderr("Wait task timeout: %s", aTaskName);
    }
  }

private:
  template <typename DispatcherType, typename Type>
  static void AddWithNotify(DispatcherType* aDispatcher,
                            nsTArray<Type*>* aList,
                            Type* aItem,
                            Monitor* aMonitor,
                            bool* aDone)
  {
    MonitorAutoLock lock(*aMonitor);

    Add(aDispatcher, aList, aItem);

    *aDone = true;
    lock.Notify();
  }

  template <typename DispatcherType, typename Type>
  static void RemoveWithNotify(DispatcherType* aDispatcher,
                               nsTArray<Type*>* aList,
                               Type* aItem,
                               Monitor* aMonitor,
                               bool* aDone)
  {
    MonitorAutoLock lock(*aMonitor);

    Remove(aDispatcher, aList, aItem);

    *aDone = true;
    lock.Notify();
  }
};

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

} // namespace mozilla

#endif // mozilla_VsyncDispatcherHelper_h
