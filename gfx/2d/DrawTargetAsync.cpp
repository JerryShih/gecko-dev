/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetAsync.h"

#include "AsyncPaintData.h"
#include "DrawCommand.h"
#include "DrawTargetTiled.h"

//#define ENABLE_PROFILER
#ifdef ENABLE_PROFILER
#include "GeckoProfiler.h"
#endif

namespace mozilla {
namespace gfx {

DrawTargetAsync::DrawTargetAsync(AsyncPaintData* aAsyncPaintData)
  : mAsyncPaintData(aAsyncPaintData)
  , mDrawCommandApplied(false)
{
  MOZ_ASSERT(aAsyncPaintData);
  mFormat = mAsyncPaintData->GetDrawTarget()->GetFormat();
}

DrawTargetAsync::~DrawTargetAsync()
{
}

bool
DrawTargetAsync::IsAsyncDrawTarget() const
{
  return true;
}

DrawTarget*
DrawTargetAsync::GetInternalDrawTarget()
{
  return mAsyncPaintData->GetDrawTarget();
}

BackendType
DrawTargetAsync::GetBackendType() const
{
  return mAsyncPaintData->GetDrawTarget()->GetBackendType();
}

DrawTargetType DrawTargetAsync::GetType() const
{
  return mAsyncPaintData->GetDrawTarget()->GetType();
}

already_AddRefed<SourceSurface>
DrawTargetAsync::Snapshot()
{
  RefPtr<SourceSurface> sourceSurface = mAsyncPaintData->GetDrawTarget()->Snapshot();

  if (sourceSurface) {
    // Since we can't make sure when the SourceSurface be used, flush all
    // pending draw command here.
    ApplyPendingDrawCommand();
  }

  return sourceSurface.forget();
}

IntSize
DrawTargetAsync::GetSize()
{
  return mAsyncPaintData->GetDrawTarget()->GetSize();
}

void
DrawTargetAsync::Flush()
{
  if (mDrawCommandApplied) {
    mDrawCommandApplied = false;
    GetInternalDrawTarget()->Flush();
  }
}

void
DrawTargetAsync::ApplyPendingDrawCommand()
{
  mDrawCommandApplied = true;
  mAsyncPaintData->ApplyPendingDrawCommand();
}

void
DrawTargetAsync::DrawSurface(SourceSurface *aSurface,
                             const Rect &aDest,
                             const Rect &aSource,
                             const DrawSurfaceOptions &aSurfOptions,
                             const DrawOptions &aOptions)
{
  {
#ifdef ENABLE_PROFILER
    PROFILER_LABEL("DrawTargetAsync", "DrawSurface::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
#endif
    printf_stderr("!!!!!!!! bignose try to call GuaranteePersistance\n");
    aSurface->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<DrawSurfaceCommand>(aSurface, aDest, aSource, aSurfOptions, aOptions);
}

void
DrawTargetAsync::DrawFilter(FilterNode *aNode,
                            const Rect &aSourceRect,
                            const Point &aDestPoint,
                            const DrawOptions &aOptions)
{
  mAsyncPaintData->AppendDrawCommand<DrawFilterCommand>(aNode, aSourceRect, aDestPoint, aOptions);
}

void
DrawTargetAsync::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                       const Point &aDest,
                                       const Color &aColor,
                                       const Point &aOffset,
                                       Float aSigma,
                                       CompositionOp aOperator)
{
  {
#ifdef ENABLE_PROFILER
    PROFILER_LABEL("DrawTargetAsync", "DrawSurfaceWithShadow::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
#endif
    printf_stderr("!!!!!!!! bignose try to call GuaranteePersistance\n");
    aSurface->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<DrawSurfaceWithShadowCommand>(aSurface, aDest, aColor, aOffset, aSigma, aOperator);
}

void
DrawTargetAsync::ClearRect(const Rect &aRect)
{
  mAsyncPaintData->AppendDrawCommand<ClearRectCommand>(aRect);
}

void
DrawTargetAsync::CopyRect(const IntRect &aSourceRect,
                          const IntPoint &aDestination)
{
  mAsyncPaintData->AppendDrawCommand<CopyRectCommand>(aSourceRect, aDestination);
}

void
DrawTargetAsync::CopySurface(SourceSurface* aSurface,
                             const IntRect& aSourceRect,
                             const IntPoint& aDestination)
{
  {
#ifdef ENABLE_PROFILER
    PROFILER_LABEL("DrawTargetAsync", "CopySurface::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
#endif
    printf_stderr("!!!!!!!! bignose try to call GuaranteePersistance\n");
    aSurface->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<CopySurfaceCommand>(aSurface, aSourceRect, aDestination);
}

void
DrawTargetAsync::FillRect(const Rect& aRect,
                          const Pattern& aPattern,
                          const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<FillRectCommand>(aRect, aPattern, aOptions);
}

void
DrawTargetAsync::StrokeRect(const Rect& aRect,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<StrokeRectCommand>(aRect, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetAsync::StrokeLine(const Point& aStart,
                            const Point& aEnd,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<StrokeLineCommand>(aStart, aEnd, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetAsync::Stroke(const Path* aPath,
                        const Pattern& aPattern,
                        const StrokeOptions& aStrokeOptions,
                        const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<StrokeCommand>(aPath, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetAsync::Fill(const Path* aPath,
                      const Pattern& aPattern,
                      const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<FillCommand>(aPath, aPattern, aOptions);
}

void
DrawTargetAsync::FillGlyphs(ScaledFont* aFont,
                            const GlyphBuffer& aBuffer,
                            const Pattern& aPattern,
                            const DrawOptions& aOptions,
                            const GlyphRenderingOptions* aRenderingOptions)
{
  mAsyncPaintData->AppendDrawCommand<FillGlyphsCommand>(aFont, aBuffer, aPattern, aOptions, aRenderingOptions);
}

void
DrawTargetAsync::Mask(const Pattern& aSource,
                      const Pattern& aMask,
                      const DrawOptions &aOptions)
{
  mAsyncPaintData->AppendDrawCommand<MaskCommand>(aSource, aMask, aOptions);
}

void
DrawTargetAsync::MaskSurface(const Pattern& aSource,
                             SourceSurface* aMask,
                             Point aOffset,
                             const DrawOptions &aOptions)
{
  {
#ifdef ENABLE_PROFILER
    PROFILER_LABEL("DrawTargetAsync", "MaskSurface::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
#endif
    printf_stderr("!!!!!!!! bignose try to call GuaranteePersistance\n");
    aMask->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<MaskSurfaceCommand>(aSource, aMask, aOffset, aOptions);
}

void
DrawTargetAsync::PushClip(const Path* aPath)
{
  mAsyncPaintData->AppendDrawCommand<PushClipCommand>(aPath);
}

void
DrawTargetAsync::PushClipRect(const Rect& aRect)
{
  mAsyncPaintData->AppendDrawCommand<PushClipRectCommand>(aRect);
}

void
DrawTargetAsync::PopClip()
{
  mAsyncPaintData->AppendDrawCommand<PopClipCommand>();
}

void
DrawTargetAsync::PushLayer(bool aOpaque,
                           Float aOpacity,
                           SourceSurface* aMask,
                           const Matrix& aMaskTransform,
                           const IntRect& aBounds,
                           bool aCopyBackground)
{
  mAsyncPaintData->AppendDrawCommand<PushLayerCommand>(aOpaque,
                                                       aOpacity,
                                                       aMask,
                                                       aMaskTransform,
                                                       aBounds,
                                                       aCopyBackground);
}

void
DrawTargetAsync::PopLayer()
{
  mAsyncPaintData->AppendDrawCommand<PopLayerCommand>();
}

already_AddRefed<SourceSurface>
DrawTargetAsync::CreateSourceSurfaceFromData(unsigned char *aData,
                                             const IntSize &aSize,
                                             int32_t aStride,
                                             SurfaceFormat aFormat) const
{
  return mAsyncPaintData->GetDrawTarget()->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
}

already_AddRefed<SourceSurface>
DrawTargetAsync::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  return mAsyncPaintData->GetDrawTarget()->OptimizeSourceSurface(aSurface);
}

already_AddRefed<SourceSurface>
DrawTargetAsync::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  return mAsyncPaintData->GetDrawTarget()->CreateSourceSurfaceFromNativeSurface(aSurface);
}

already_AddRefed<DrawTarget>
DrawTargetAsync::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  return mAsyncPaintData->GetDrawTarget()->CreateSimilarDrawTarget(aSize, aFormat);
}

already_AddRefed<DrawTarget>
DrawTargetAsync::CreateShadowDrawTarget(const IntSize &aSize,
                                        SurfaceFormat aFormat,
                                        float aSigma) const
{
  return mAsyncPaintData->GetDrawTarget()->CreateShadowDrawTarget(aSize, aFormat, aSigma);
}

already_AddRefed<PathBuilder>
DrawTargetAsync::CreatePathBuilder(FillRule aFillRule) const
{
  return mAsyncPaintData->GetDrawTarget()->CreatePathBuilder(aFillRule);
}

already_AddRefed<GradientStops>
DrawTargetAsync::CreateGradientStops(GradientStop *aStops,
                                     uint32_t aNumStops,
                                     ExtendMode aExtendMode) const
{
  return mAsyncPaintData->GetDrawTarget()->CreateGradientStops(aStops, aNumStops, aExtendMode);
}

already_AddRefed<FilterNode>
DrawTargetAsync::CreateFilter(FilterType aType)
{
  return mAsyncPaintData->GetDrawTarget()->CreateFilter(aType);
}

void
DrawTargetAsync::SetTransform(const Matrix& aTransform)
{
  mTransform = aTransform;

  DrawingCommand* cmd = mAsyncPaintData->GetLastCommand();
  if (cmd && cmd->GetType() == CommandType::SETTRANSFORM) {
    static_cast<SetTransformCommand*>(cmd)->SetTransform(aTransform);

    return;
  }

  mAsyncPaintData->AppendDrawCommand<SetTransformCommand>(aTransform);
}

void*
DrawTargetAsync::GetNativeSurface(NativeSurfaceType aType)
{
  void* nativeSurface = mAsyncPaintData->GetDrawTarget()->GetNativeSurface(aType);

  if (nativeSurface) {
    // Since we can't make sure when the NativeSurface be used, flush all
    // pending draw command here.
    ApplyPendingDrawCommand();
  }

  return nativeSurface;
}

bool
DrawTargetAsync::IsDualDrawTarget() const
{
  return mAsyncPaintData->GetDrawTarget()->IsDualDrawTarget();
}

bool
DrawTargetAsync::IsTiledDrawTarget() const
{
  return mAsyncPaintData->GetDrawTarget()->IsTiledDrawTarget();
}

bool
DrawTargetAsync::SupportsRegionClipping() const
{
  return mAsyncPaintData->GetDrawTarget()->SupportsRegionClipping();
}

void
DrawTargetAsync::SetOpaqueRect(const IntRect &aRect)
{
  mOpaqueRect = aRect;
  mAsyncPaintData->AppendDrawCommand<SetOpaqueRectCommand>(aRect);
}

void
DrawTargetAsync::SetPermitSubpixelAA(bool aPermitSubpixelAA)
{
  mPermitSubpixelAA = aPermitSubpixelAA;
  mAsyncPaintData->AppendDrawCommand<SetPermitSubpixelAACommand>(aPermitSubpixelAA);
}

#ifdef USE_SKIA_GPU
bool
DrawTargetAsync::InitWithGrContext(GrContext* aGrContext,
                                   const IntSize &aSize,
                                   SurfaceFormat aFormat)
{
  return mAsyncPaintData->GetDrawTarget()->InitWithGrContext(aGrContext, aSize, aFormat);
}
#endif

DrawTargetTiledAsync::DrawTargetTiledAsync(AsyncPaintData* aAsyncPaintData)
  : DrawTargetAsync(aAsyncPaintData)
{
}

DrawTargetTiledAsync::~DrawTargetTiledAsync()
{
}

bool
DrawTargetTiledAsync::Init(const TileSet& aTiles)
{
  MOZ_ASSERT(GetInternalDrawTarget());
  MOZ_ASSERT(GetInternalDrawTarget()->IsTiledDrawTarget());

  if (!aTiles.mTileCount) {
    return false;
  }

  for (size_t i = 0; i < aTiles.mTileCount; ++i) {
    if (!aTiles.mTiles[i].mDrawTarget->IsAsyncDrawTarget()) {
      return false;
    }
  }

  // replace tiles' dt with internal dt
  std::vector<Tile> tiles(aTiles.mTileCount);
  for (size_t i = 0; i < aTiles.mTileCount; ++i) {
    tiles[i].mDrawTarget =
        static_cast<DrawTargetAsync*>(aTiles.mTiles[i].mDrawTarget.get())->GetInternalDrawTarget();
    tiles[i].mTileOrigin = aTiles.mTiles[i].mTileOrigin;
    MOZ_ASSERT(tiles[i].mDrawTarget);
    MOZ_ASSERT(!tiles[i].mDrawTarget->IsAsyncDrawTarget());
  }

  TileSet tileSet = { &tiles[0], aTiles.mTileCount };

  if (!static_cast<DrawTargetTiled*>(GetInternalDrawTarget())->Init(tileSet)) {
    return false;
  }

  mFormat = tiles[0].mDrawTarget->GetFormat();

  return true;
}

bool
DrawTargetTiledAsync::IsTiledDrawTarget() const
{
  return true;
}

} // namespace gfx
} // namespace mozilla
