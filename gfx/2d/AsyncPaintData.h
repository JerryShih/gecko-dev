/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_ASYNCPAINTDATA_H_
#define MOZILLA_GFX_ASYNCPAINTDATA_H_

#include <vector>

#include "IterableArena.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gfx {

class DrawTarget;
class DrawTargetAsync;
class DrawingCommand;

// Used for recording drawTarget draw command and replaying.
class AsyncPaintData : public external::AtomicRefCounted<AsyncPaintData>
{
  friend class DrawTargetAsync;

public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(AsyncPaintData);

  AsyncPaintData();
  virtual ~AsyncPaintData();

  template<typename CommandType, typename... Args>
  void AppendDrawCommand(Args&&... aArgs)
  {
    static_assert(IsBaseOf<DrawingCommand, CommandType>::value,
                  "CommandType must derive from DrawingCommand");

    InitPool();

    ptrdiff_t index = mCommandPool->Alloc<CommandType>(Forward<Args>(aArgs)...);
    mPendingDrawCommandIndex.push_back(index);
  }

  // Return nullptr if there is no command in buffer.
  DrawingCommand* GetLastCommand();

  void ApplyPendingDrawCommand();

  // Get current ref drawTarget. Should be called at main thread.
  virtual DrawTarget* GetDrawTarget() = 0;

  // Prepare the drawTarget for GetDrawTargetForAsyncPainting().
  virtual void LockForAsyncPainting() = 0;
  virtual void UnlockForAsyncPainting() = 0;
  // Get current ref drawTarget.
  virtual DrawTarget* GetDrawTargetForAsyncPainting() = 0;

private:
  void InitPool();
  void ClearPoolDrawCommand();
  void ReleasePool();

  std::vector<ptrdiff_t> mPendingDrawCommandIndex;
  UniquePtr<IterableArena> mCommandPool;

  bool mIsPoolReady;
};

// Holding a DrawTarget for AsyncPaintData drawing.
class DrawTargetAsyncPaintData final : public AsyncPaintData
{
public:
  explicit DrawTargetAsyncPaintData(DrawTarget* aRefDrawTarget);

  virtual ~DrawTargetAsyncPaintData();

  virtual DrawTarget* GetDrawTarget() override;

  virtual void LockForAsyncPainting() override;

  virtual void UnlockForAsyncPainting() override;

  virtual DrawTarget* GetDrawTargetForAsyncPainting() override;

private:
  RefPtr<DrawTarget> mRefDrawTarget;
};

} //namespace gfx
} //namespace mozilla

#endif  //MOZILLA_GFX_ASYNCPAINTDATA_H_
