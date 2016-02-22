#include "DrawTargetAsyncManager.h"

#include "base/thread.h"
#include "base/waitable_event.h"
#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/gfx/AsyncPaintData.h"

namespace mozilla {
namespace gfx {

DrawTargetAsyncManager::DrawTargetAsyncManager()
  : mLastmTransactionIndex(-1)
  , mInTransaction(false)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOffManPaintingThread) {
    mOffManPaintingThread.reset(new base::Thread("OffMainPaintingThread"));

    MOZ_ASSERT(mOffManPaintingThread.get());
    MOZ_ALWAYS_TRUE(mOffManPaintingThread->Start());
  }
}

DrawTargetAsyncManager::~DrawTargetAsyncManager()
{

}

void
DrawTargetAsyncManager::BeginTransaction()
{
  ++mLastmTransactionIndex;

  MOZ_ASSERT(!mInTransaction);
  MOZ_ASSERT(mLastmTransactionIndex < MAX_TRANSACTION_NUM);
  MOZ_ASSERT(mTransactions.Length() <= MAX_TRANSACTION_NUM);
  MOZ_ASSERT(mLastmTransactionIndex <= mTransactions.Length());

  mInTransaction = true;

  if (mLastmTransactionIndex + 1 > static_cast<int32_t>(mTransactions.Length())) {
    mTransactions.AppendElement(UniquePtr<Transaction>(new Transaction));
  }
}

void
DrawTargetAsyncManager::AppendAsyncPaintData(AsyncPaintData* aAsyncPaintData)
{
  MOZ_ASSERT(mInTransaction);
  MOZ_ASSERT(mLastmTransactionIndex >= 0);
  MOZ_ASSERT(mLastmTransactionIndex < MAX_TRANSACTION_NUM);
  MOZ_ASSERT(mTransactions[mLastmTransactionIndex]);

  mTransactions[mLastmTransactionIndex]->mPaintData.AppendElement(aAsyncPaintData);
}

void
DrawTargetAsyncManager::EndTransaction()
{
  MOZ_ASSERT(mInTransaction);
  MOZ_ASSERT(mLastmTransactionIndex >= 0);

  mInTransaction = false;
}

void
DrawTargetAsyncManager::ApplyTransaction(Transaction* aTransaction, ipc::MessageChannel* aMessageChannel)
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(aTransaction->mFired);

#ifdef PROFILE_DRAW_COMMAND
  ATRACE_NAME("DrawTargetAsyncManager::ApplyPendingDrawCommand");
#endif

  printf_stderr("!!!bignose DrawTargetAsyncManager::ApplyTransaction, target num:%u",aTransaction->mPaintData.Length());

  auto num = aTransaction->mPaintData.Length();
  for (decltype(num) i = 0 ; i<num ; ++i) {
    aTransaction->mPaintData[i]->ApplyPendingDrawCommand();
  }
  aTransaction->mPaintData.Clear();

  if (aMessageChannel) {
    printf_stderr("bignose EndDeferring");
    aMessageChannel->EndDeferring();
  }

  aTransaction->mWaitableEvent->Signal();
}

void
DrawTargetAsyncManager::ApplyLastTransaction()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInTransaction);
  MOZ_ASSERT(mLastmTransactionIndex >= 0);
  MOZ_ASSERT(!mTransactions[mLastmTransactionIndex]->mFired);

  mTransactions[mLastmTransactionIndex]->mFired = true;
  ApplyTransaction(mTransactions[mLastmTransactionIndex].get(), nullptr);
}

void
DrawTargetAsyncManager::PostApplyLastTransaction(ipc::MessageChannel* aMessageChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInTransaction);
  MOZ_ASSERT(mLastmTransactionIndex >= 0);
  MOZ_ASSERT(!mTransactions[mLastmTransactionIndex]->mFired);
  MOZ_ASSERT(mOffManPaintingThread.get());
  MOZ_ASSERT(mOffManPaintingThread->message_loop());

  printf_stderr("bignose StartDeferring");
  MOZ_ALWAYS_TRUE(aMessageChannel->StartDeferring());
  mTransactions[mLastmTransactionIndex]->mFired = true;
  mOffManPaintingThread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &DrawTargetAsyncManager::ApplyTransaction,
                        mTransactions[mLastmTransactionIndex].get(),
                        aMessageChannel));
}

void
DrawTargetAsyncManager::WaitAllTransaction()
{
  MOZ_ASSERT(!mInTransaction);

  PROFILER_LABEL("DrawTargetAsyncManager", "WaitPaintingTask",
    js::ProfileEntry::Category::GRAPHICS);

  if (mLastmTransactionIndex < 0) {
    return;
  }

  for (int32_t i = mLastmTransactionIndex; i >= 0; --i) {
    if (mTransactions[i]->mFired && !mTransactions[i]->mWaitableEvent->IsSignaled()) {
      mTransactions[i]->mWaitableEvent->Wait();

      //bignose
      //use TimedWait() instead?
      //mTransactions[i]->mWaitableEvent->TimedWait(TimeDelta::FromSeconds(5));
    }
    mTransactions[i]->mPaintData.Clear();
    mTransactions[i]->mWaitableEvent->Reset();
    mTransactions[i]->mFired = false;
  }

  mLastmTransactionIndex = -1;

  ClearResource();
}

void
DrawTargetAsyncManager::ClearResource()
{
  PROFILER_LABEL("DrawTargetAsyncManager", "ClearResource",
    js::ProfileEntry::Category::GRAPHICS);
}

DrawTargetAsyncManager::Transaction::Transaction()
  : mWaitableEvent(new base::WaitableEvent(true, false))
  , mFired(false)
{
}

DrawTargetAsyncManager::Transaction::~Transaction()
{
}

} //namespace gfx
} //namespace mozilla
