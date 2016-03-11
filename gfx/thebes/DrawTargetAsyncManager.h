/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_DrawTargetAsyncManager_h
#define mozilla_gfx_thebes_DrawTargetAsyncManager_h

#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace base {
class Thread;
class WaitableEvent;
}

namespace mozilla {
namespace ipc {
class MessageChannel;
}
}

namespace mozilla {
namespace gfx {

class AsyncPaintData;

class DrawTargetAsyncManager final : public external::AtomicRefCounted<DrawTargetAsyncManager>
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetAsyncManager);

  DrawTargetAsyncManager();
  virtual ~DrawTargetAsyncManager();

  void BeginTransaction();
  void AppendAsyncPaintData(AsyncPaintData* aAsyncPaintData);
  void EndTransaction();

  void ApplyLastTransaction();
  void PostApplyLastTransaction(ipc::MessageChannel* aMessageChannel);

  void WaitAllTransaction();

  void ClearResource();

private:
  const uint32_t MAX_TRANSACTION_NUM = 8;

  class Transaction
  {
  public:
    Transaction();
    ~Transaction();

    nsTArray<RefPtr<AsyncPaintData>> mPaintData;
    UniquePtr<base::WaitableEvent> mWaitableEvent;
    bool mFired;
  };

  void ApplyTransaction(Transaction* aTransaction, ipc::MessageChannel* aMessageChannel);

  UniquePtr<base::Thread> mOffManPaintingThread;

  nsTArray<UniquePtr<Transaction>> mTransactions;
  int32_t mLastmTransactionIndex;
  bool mInTransaction;
};

} //namespace gfx
} //namespace mozilla

#endif //mozilla_gfx_thebes_DrawTargetAsyncManager_h
