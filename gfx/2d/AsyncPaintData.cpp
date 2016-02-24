/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncPaintData.h"

#include "2D.h"
#include "DrawCommand.h"
#include "DrawTargetAsyncManager.h"

#include "gfxPlatform.h"

//#define ENABLE_PROFILER
#ifdef ENABLE_PROFILER
#include "GeckoProfiler.h"
#endif

namespace mozilla {
namespace gfx {

AsyncPaintData::AsyncPaintData()
  : mIsPoolReady(false)
{
}

AsyncPaintData::~AsyncPaintData()
{
  ReleasePool();
}

void
AsyncPaintData::InitPool()
{
  if (mIsPoolReady) {
    return;
  }

#ifdef ENABLE_PROFILER
  PROFILER_LABEL("AsyncPaintData", "InitPool", js::ProfileEntry::Category::GRAPHICS);
#endif

  const uint32_t cmdBufferSize = 512;
  mCommandPool.reset(new IterableArena(IterableArena::ArenaType::GROWABLE, cmdBufferSize));
  mPendingDrawCommandIndex.reserve(32);

  mIsPoolReady = true;
}

void
AsyncPaintData::ClearPoolDrawCommand()
{
  if (!mIsPoolReady) {
    return;
  }

  {
#ifdef ENABLE_PROFILER
    PROFILER_LABEL("AsyncPaintData", "ClearPoolDrawCommand::~DrawingCommand", js::ProfileEntry::Category::GRAPHICS);
#endif

    auto num = mPendingDrawCommandIndex.size();
    for (decltype(num) i = 0 ; i<num ; ++i) {
      static_cast<DrawingCommand*>(mCommandPool->GetStorage(mPendingDrawCommandIndex[i]))->~DrawingCommand();
    }
    mCommandPool->Clear();
    mPendingDrawCommandIndex.clear();
  }
}

void
AsyncPaintData::ReleasePool()
{
  if (!mIsPoolReady) {
    return;
  }

  ClearPoolDrawCommand();
}

DrawingCommand*
AsyncPaintData::GetLastCommand()
{
  if (mPendingDrawCommandIndex.empty()) {
    return nullptr;
  }

  ptrdiff_t index = mPendingDrawCommandIndex.back();
  return (DrawingCommand*)(mCommandPool->GetStorage(index));
}

void
AsyncPaintData::ApplyPendingDrawCommand()
{
#ifdef ENABLE_PROFILER
  PROFILER_LABEL("AsyncPaintData", "ApplyPendingDrawCommand", js::ProfileEntry::Category::GRAPHICS);
#endif

  auto commandNum = mPendingDrawCommandIndex.size();

  if (!commandNum) {
    return;
  }

  LockForAsyncPainting();

  DrawTarget* drawTarget = GetDrawTargetForAsyncPainting();
  MOZ_ASSERT(drawTarget);

  for (decltype(commandNum) i = 0 ; i < commandNum ; ++i) {
    static_cast<DrawingCommand*>(mCommandPool->GetStorage(mPendingDrawCommandIndex[i]))->ExecuteOnDT(drawTarget, nullptr);
  }
  ClearPoolDrawCommand();

  UnlockForAsyncPainting();
}

DrawTargetAsyncPaintData::DrawTargetAsyncPaintData(DrawTarget* aRefDrawTarget)
  : mRefDrawTarget(aRefDrawTarget)
{
  MOZ_ASSERT(aRefDrawTarget);
  MOZ_ASSERT(!aRefDrawTarget->IsAsyncDrawTarget());
  MOZ_ASSERT(gfxPlatform::GetPlatform()->GetDrawTargetAsyncManager());
  gfxPlatform::GetPlatform()->GetDrawTargetAsyncManager()->AppendAsyncPaintData(this);
}

DrawTargetAsyncPaintData::~DrawTargetAsyncPaintData()
{
  mRefDrawTarget = nullptr;
}

DrawTarget*
DrawTargetAsyncPaintData::GetDrawTarget()
{
  return mRefDrawTarget;
}

void
DrawTargetAsyncPaintData::LockForAsyncPainting()
{
  // do nothing since we already have dt.
}

void
DrawTargetAsyncPaintData::UnlockForAsyncPainting()
{
  // do nothing since we already have dt.
}

DrawTarget*
DrawTargetAsyncPaintData::GetDrawTargetForAsyncPainting()
{
  return mRefDrawTarget;
}

} //namespace gfx
} //namespace mozilla

