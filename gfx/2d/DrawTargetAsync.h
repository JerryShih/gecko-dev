/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_ASYNCDRAWTARGET_H_
#define MOZILLA_GFX_ASYNCDRAWTARGET_H_

#include <vector>

#include "2D.h"
#include "Filters.h"
#include "plarena.h"

#include "GeckoProfiler.h"

namespace mozilla {
namespace gfx {

class AsyncDrawCommand;
class AsyncDrawTargetManager;
class AsyncPaintData;
class DrawTargetAsync;
class DrawTargetCairo;
class DrawTargetCG;

class AsyncPaintData : public RefCounted<AsyncPaintData>
{
  friend class AsyncDrawTargetManager;
  friend class DrawTargetAsync;

public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(AsyncPaintData);

  AsyncPaintData();
  virtual ~AsyncPaintData();

  template<typename CommandType, typename... Args>
  void AppendDrawCommand(Args&&... aArgs)
  {
    InitPool();

    void* commandAddr = nullptr;
    PL_ARENA_ALLOCATE(commandAddr, &mPool, sizeof(CommandType));
    MOZ_ASSERT(commandAddr);

    mPendingDrawCommand.push_back(reinterpret_cast<CommandType*>(commandAddr));

    new (commandAddr) CommandType(Forward<Args>(aArgs)...);

//    //bignose
//    //test flush immediately
//    printf_stderr("bignose Test flush start");
    TestDrawCommand();
//    printf_stderr("bignose Test flush end");
  }

  void TestDrawCommand();

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

  PLArenaPool mPool;
  std::vector<AsyncDrawCommand*> mPendingDrawCommand;
  bool mIsPoolReady;
};

class DrawTargetAsync final : public DrawTarget
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetAsync, override);

  DrawTargetAsync(AsyncPaintData* aAsyncPaintData);
  virtual ~DrawTargetAsync();

  void ApplyPendingDrawCommand();

  virtual bool IsAsyncDrawTarget() const override;

  DrawTarget* GetInternalDrawTarget();

  virtual BackendType GetBackendType() const override;
  virtual DrawTargetType GetType() const override;

  // Snapshot will flush all pending draw command to its internal drawTarget.
  virtual already_AddRefed<SourceSurface> Snapshot() override;

  virtual IntSize GetSize() override;

  virtual void Flush() override;

  virtual void DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions,
                           const DrawOptions &aOptions) override;

  virtual void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions = DrawOptions()) override;

  virtual void DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator) override;

  virtual void ClearRect(const Rect &aRect) override;

  virtual void CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination) override;

  virtual void CopyRect(const IntRect &aSourceRect,
                        const IntPoint &aDestination) override;

  virtual void FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions = DrawOptions()) override;

  virtual void StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) override;

  virtual void StrokeLine(const Point &aStart,
                          const Point &aEnd,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions = StrokeOptions(),
                          const DrawOptions &aOptions = DrawOptions()) override;

  virtual void Stroke(const Path *aPath,
                      const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions = StrokeOptions(),
                      const DrawOptions &aOptions = DrawOptions()) override;

  virtual void Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions = DrawOptions()) override;

  virtual void FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions = DrawOptions(),
                          const GlyphRenderingOptions *aRenderingOptions = nullptr) override;

  virtual void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions = DrawOptions()) override;

  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions()) override;

  virtual void PushClip(const Path *aPath) override;
  virtual void PushClipRect(const Rect &aRect) override;
  virtual void PopClip() override;

  virtual already_AddRefed<SourceSurface>
  CreateSourceSurfaceFromData(unsigned char *aData,
                              const IntSize &aSize,
                              int32_t aStride,
                              SurfaceFormat aFormat) const override;

  virtual already_AddRefed<SourceSurface>
  OptimizeSourceSurface(SourceSurface *aSurface) const override;

  virtual already_AddRefed<SourceSurface>
  CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override;

  virtual already_AddRefed<DrawTarget>
  CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override;

  // No DrawTargetCapture for AsyncDrawTarget
  virtual already_AddRefed<DrawTargetCapture> CreateCaptureDT(const IntSize& aSize) override
  {
    return nullptr;
  }

  virtual already_AddRefed<DrawTargetCapture> CreateCaptureDT()
  {
    return nullptr;
  }

  virtual already_AddRefed<DrawTarget> CreateShadowDrawTarget(const IntSize &aSize,
                                                              SurfaceFormat aFormat,
                                                              float aSigma) const override;

  virtual already_AddRefed<PathBuilder>
  CreatePathBuilder(FillRule aFillRule = FillRule::FILL_WINDING) const override;

  virtual already_AddRefed<GradientStops>
  CreateGradientStops(GradientStop *aStops,
                      uint32_t aNumStops,
                      ExtendMode aExtendMode = ExtendMode::CLAMP) const override;

  virtual already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;

  virtual void SetTransform(const Matrix &aTransform) override;

  virtual void *GetNativeSurface(NativeSurfaceType aType) override;

  virtual bool IsDualDrawTarget() const override;
  virtual bool IsTiledDrawTarget() const override;
  virtual bool SupportsRegionClipping() const override;

  virtual void SetOpaqueRect(const IntRect &aRect) override;

  virtual void SetPermitSubpixelAA(bool aPermitSubpixelAA) override;

#ifdef USE_SKIA_GPU
  virtual bool InitWithGrContext(GrContext* aGrContext, const IntSize &aSize, SurfaceFormat aFormat) override;
#endif

private:
  RefPtr<AsyncPaintData> mAsyncPaintData;
  bool mDrawCommandApplied;
};

} // namespace gfx
} // namespace mozilla
#endif /* MOZILLA_GFX_ASYNCDRAWTARGET_H_ */
