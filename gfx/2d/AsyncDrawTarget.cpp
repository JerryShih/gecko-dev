/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncDrawTarget.h"
#include "DrawCommand.h"
#include "GeckoProfiler.h"
#include "mozilla/dom/UnionMember.h"



#ifndef ATRACE_TAG
# define ATRACE_TAG ATRACE_TAG_ALWAYS
#endif

// We need HAVE_ANDROID_OS to be defined for Trace.h.
// If its not set we will set it temporary and remove it.
# ifndef HAVE_ANDROID_OS
#  define HAVE_ANDROID_OS
#  define REMOVE_HAVE_ANDROID_OS
# endif

// Android source code will include <cutils/trace.h> before this. There is no
// HAVE_ANDROID_OS defined in Firefox OS build at that time. Enabled it globally
// will cause other build break. So atrace_begin and atrace_end are not defined.
// It will cause a build-break when we include <utils/Trace.h>. Use undef
// _LIBS_CUTILS_TRACE_H will force <cutils/trace.h> to define atrace_begin and
// atrace_end with defined HAVE_ANDROID_OS again. Then there is no build-break.
# undef _LIBS_CUTILS_TRACE_H
# include <utils/Trace.h>
# ifdef REMOVE_HAVE_ANDROID_OS
#  undef HAVE_ANDROID_OS
#  undef REMOVE_HAVE_ANDROID_OS
# endif




namespace mozilla {
namespace gfx {

#define AppendCommand(arg) new (AppendToCommandList<arg>()) arg

enum class AsyncDrawCommandType : int8_t {
  DRAWSURFACE = 0,
  DRAWFILTER,
  DRAWSURFACEWITHSHADOW,
  CLEARRECT,
  COPYSURFACE,
  COPYRECT,
  FILLRECT,
  STROKERECT,
  STROKELINE,
  STROKE,
  FILL,
  FILLGLYPHS,
  MASK,
  MASKSURFACE,
  PUSHCLIP,
  PUSHCLIPRECT,
  POPCLIP,
  SETTRANSFORM,
  SETOPAQUERECT,
  SETPERMITSUBPIXELAA,
};

class AsyncDrawCommand
{
public:
  explicit AsyncDrawCommand(AsyncDrawCommandType aType)
    : mType(aType)
  {
  }

  virtual ~AsyncDrawCommand() {}

  virtual void ExecuteOnDT(DrawTarget* aDT) = 0;

  AsyncDrawCommandType GetType() { return mType; }

  //void* operator new(size_t aSize, PLArenaPool* aArena);

private:
  AsyncDrawCommandType mType;
};

class AsyncDrawCommandPatternData
{
public:
  AsyncDrawCommandPatternData() = delete;

  explicit AsyncDrawCommandPatternData(const Pattern& aPattern)
  {
    Assign(aPattern);
  }

  ~AsyncDrawCommandPatternData()
  {
    Delete();
  }

  operator Pattern&()
  {
    switch (mType) {
      case PatternType::COLOR: {
        return mData.mColor.Value();
      }
      case PatternType::SURFACE: {
        return mData.mSurface.Value();
      }
      case PatternType::LINEAR_GRADIENT: {
        return mData.mLinear.Value();
      }
      case PatternType::RADIAL_GRADIENT: {
        return mData.mRadial.Value();
      }
      default:
        MOZ_CRASH("Non-support pattern type");
    }

    return mData.mColor.Value();
  }

  operator const Pattern&() const
  {
    switch (mType) {
      case PatternType::COLOR: {
        return mData.mColor.Value();
      }
      case PatternType::SURFACE: {
        return mData.mSurface.Value();
      }
      case PatternType::LINEAR_GRADIENT: {
        return mData.mLinear.Value();
      }
      case PatternType::RADIAL_GRADIENT: {
        return mData.mRadial.Value();
      }
      default:
        MOZ_CRASH("Non-support pattern type");
    }

    return mData.mColor.Value();
  }

  // Block this so that we notice if someone's doing excessive assigning.
  AsyncDrawCommandPatternData operator=(const AsyncDrawCommandPatternData& aOther) = delete;

private:
  void Assign(const Pattern& aPattern)
  {
    mType = aPattern.GetType();

    switch (mType) {
      case PatternType::COLOR: {
        mData.mColor.SetValue(*static_cast<const ColorPattern*>(&aPattern));
        break;
      }
      case PatternType::SURFACE: {
        SurfacePattern& surfPat = mData.mSurface.SetValue(*static_cast<const SurfacePattern*>(&aPattern));
        ATRACE_NAME("AsyncDrawCommandPatternData::GuaranteePersistance");
        surfPat.mSurface->GuaranteePersistance();
        break;
      }
      case PatternType::LINEAR_GRADIENT: {
        mData.mLinear.SetValue(*static_cast<const LinearGradientPattern*>(&aPattern));
        break;
      }
      case PatternType::RADIAL_GRADIENT: {
        mData.mRadial.SetValue(*static_cast<const RadialGradientPattern*>(&aPattern));
        break;
      }
      default:
        MOZ_CRASH("Non-support pattern type");
    }
  }

  void Delete()
  {
    switch (mType) {
      case PatternType::COLOR: {
        mData.mColor.Destroy();
        break;
      }
      case PatternType::SURFACE: {
        mData.mSurface.Destroy();
        break;
      }
      case PatternType::LINEAR_GRADIENT: {
        mData.mLinear.Destroy();
        break;
      }
      case PatternType::RADIAL_GRADIENT: {
        mData.mRadial.Destroy();
        break;
      }
      default:
        MOZ_CRASH("Non-support pattern type");
    }
  }

  union PatternData{
    dom::UnionMember<ColorPattern> mColor;
    dom::UnionMember<SurfacePattern> mSurface;
    dom::UnionMember<LinearGradientPattern> mLinear;
    dom::UnionMember<RadialGradientPattern> mRadial;
  };

  PatternType mType;
  PatternData mData;
};

class AsyncDrawSurfaceCommand : public AsyncDrawCommand
{
public:
  AsyncDrawSurfaceCommand(SourceSurface *aSurface,
                          const Rect& aDest,
                          const Rect& aSource,
                          const DrawSurfaceOptions& aSurfOptions,
                          const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::DRAWSURFACE)
    , mSurface(aSurface)
    , mDest(aDest)
    , mSource(aSource)
    , mSurfOptions(aSurfOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->DrawSurface(mSurface, mDest, mSource, mSurfOptions, mOptions);
  }

private:
  RefPtr<SourceSurface> mSurface;
  Rect mDest;
  Rect mSource;
  DrawSurfaceOptions mSurfOptions;
  DrawOptions mOptions;
};

class AsyncDrawFilterCommand : public AsyncDrawCommand
{
public:
  AsyncDrawFilterCommand(FilterNode* aFilter,
                         const Rect& aSourceRect,
                         const Point& aDestPoint,
                         const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::DRAWSURFACE)
    , mFilter(aFilter)
    , mSourceRect(aSourceRect)
    , mDestPoint(aDestPoint)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->DrawFilter(mFilter, mSourceRect, mDestPoint, mOptions);
  }

private:
  RefPtr<FilterNode> mFilter;
  Rect mSourceRect;
  Point mDestPoint;
  DrawOptions mOptions;
};

class AsyncDrawSurfaceWithShadowCommand : public AsyncDrawCommand
{
public:
  explicit AsyncDrawSurfaceWithShadowCommand(SourceSurface *aSurface,
                                             const Point &aDest,
                                             const Color &aColor,
                                             const Point &aOffset,
                                             Float aSigma,
                                             CompositionOp aOperator)
    : AsyncDrawCommand(AsyncDrawCommandType::DRAWSURFACEWITHSHADOW)
    , mSurface(aSurface)
    , mDest(aDest)
    , mColor(aColor)
    , mOffset(aOffset)
    , mSigma(aSigma)
    , mOperator(aOperator)
  {

  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->DrawSurfaceWithShadow(mSurface, mDest, mColor, mOffset, mSigma, mOperator);
  }

private:
  RefPtr<SourceSurface> mSurface;
  Point mDest;
  Color mColor;
  Point mOffset;
  Float mSigma;
  CompositionOp mOperator;
};

class AsyncClearRectCommand : public AsyncDrawCommand
{
public:
  explicit AsyncClearRectCommand(const Rect& aRect)
    : AsyncDrawCommand(AsyncDrawCommandType::CLEARRECT)
    , mRect(aRect)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->ClearRect(mRect);
  }

private:
  Rect mRect;
};

class AsyncCopyRectCommand : public AsyncDrawCommand
{
public:
  AsyncCopyRectCommand(const IntRect &aSourceRect, const IntPoint &aDestination)
    : AsyncDrawCommand(AsyncDrawCommandType::COPYRECT)
    , mSourceRect(aSourceRect)
    , mDestination(aDestination)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->CopyRect(mSourceRect, mDestination);
  }

private:
  IntRect mSourceRect;
  IntPoint mDestination;
};

class AsyncCopySurfaceCommand : public AsyncDrawCommand
{
public:
  AsyncCopySurfaceCommand(SourceSurface* aSurface,
                          const IntRect& aSourceRect,
                          const IntPoint& aDestination)
    : AsyncDrawCommand(AsyncDrawCommandType::COPYSURFACE)
    , mSurface(aSurface)
    , mSourceRect(aSourceRect)
    , mDestination(aDestination)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    Point dest(Float(mDestination.x), Float(mDestination.y));
    aDT->CopySurface(mSurface, mSourceRect, IntPoint(uint32_t(dest.x), uint32_t(dest.y)));
  }

private:
  RefPtr<SourceSurface> mSurface;
  IntRect mSourceRect;
  IntPoint mDestination;
};

class AsyncFillRectCommand : public AsyncDrawCommand
{
public:
  AsyncFillRectCommand(const Rect& aRect,
                       const Pattern& aPattern,
                       const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::FILLRECT)
    , mRect(aRect)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->FillRect(mRect, mPattern, mOptions);
  }

private:
  Rect mRect;
  AsyncDrawCommandPatternData mPattern;
  DrawOptions mOptions;
};

class AsyncStrokeRectCommand : public AsyncDrawCommand
{
public:
  AsyncStrokeRectCommand(const Rect& aRect,
                         const Pattern& aPattern,
                         const StrokeOptions& aStrokeOptions,
                         const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::STROKERECT)
    , mRect(aRect)
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->StrokeRect(mRect, mPattern, mStrokeOptions, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix&)
  {
    aDT->StrokeRect(mRect, mPattern, mStrokeOptions, mOptions);
  }

private:
  Rect mRect;
  AsyncDrawCommandPatternData mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class AsyncStrokeLineCommand : public AsyncDrawCommand
{
public:
  AsyncStrokeLineCommand(const Point& aStart,
                         const Point& aEnd,
                         const Pattern& aPattern,
                         const StrokeOptions& aStrokeOptions,
                         const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::STROKELINE)
    , mStart(aStart)
    , mEnd(aEnd)
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->StrokeLine(mStart, mEnd, mPattern, mStrokeOptions, mOptions);
  }

private:
  Point mStart;
  Point mEnd;
  AsyncDrawCommandPatternData mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class AsyncFillCommand : public AsyncDrawCommand
{
public:
  AsyncFillCommand(const Path* aPath,
                   const Pattern& aPattern,
                   const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::FILL)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->Fill(mPath, mPattern, mOptions);
  }

private:
  RefPtr<Path> mPath;
  AsyncDrawCommandPatternData mPattern;
  DrawOptions mOptions;
};

class AsyncStrokeCommand : public AsyncDrawCommand
{
public:
  AsyncStrokeCommand(const Path* aPath,
                     const Pattern& aPattern,
                     const StrokeOptions& aStrokeOptions,
                     const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::STROKE)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->Stroke(mPath, mPattern, mStrokeOptions, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix&)
  {
    aDT->Stroke(mPath, mPattern, mStrokeOptions, mOptions);
  }

private:
  RefPtr<Path> mPath;
  AsyncDrawCommandPatternData mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class AsyncFillGlyphsCommand : public AsyncDrawCommand
{
public:
  AsyncFillGlyphsCommand(ScaledFont* aFont,
                         const GlyphBuffer& aBuffer,
                         const Pattern& aPattern,
                         const DrawOptions& aOptions,
                         const GlyphRenderingOptions* aRenderingOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::FILLGLYPHS)
    , mFont(aFont)
    , mPattern(aPattern)
    , mOptions(aOptions)
    , mRenderingOptions(const_cast<GlyphRenderingOptions*>(aRenderingOptions))
  {
    mGlyphs.resize(aBuffer.mNumGlyphs);
    memcpy(&mGlyphs.front(), aBuffer.mGlyphs, sizeof(Glyph) * aBuffer.mNumGlyphs);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    GlyphBuffer buf = { &mGlyphs.front(), mGlyphs.size()};
    aDT->FillGlyphs(mFont, buf, mPattern, mOptions, mRenderingOptions);
  }

private:
  RefPtr<ScaledFont> mFont;
  std::vector<Glyph> mGlyphs;
  AsyncDrawCommandPatternData mPattern;
  DrawOptions mOptions;
  RefPtr<GlyphRenderingOptions> mRenderingOptions;
};

class AsyncMaskCommand : public AsyncDrawCommand
{
public:
  AsyncMaskCommand(const Pattern& aSource,
                   const Pattern& aMask,
                   const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::MASK)
    , mSource(aSource)
    , mMask(aMask)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->Mask(mSource, mMask, mOptions);
  }

private:
  AsyncDrawCommandPatternData mSource;
  AsyncDrawCommandPatternData mMask;
  DrawOptions mOptions;
};

class AsyncMaskSurfaceCommand : public AsyncDrawCommand
{
public:
  AsyncMaskSurfaceCommand(const Pattern& aSource,
                          const SourceSurface* aMask,
                          const Point& aOffset,
                          const DrawOptions& aOptions)
    : AsyncDrawCommand(AsyncDrawCommandType::MASKSURFACE)
    , mSource(aSource)
    , mMask(const_cast<SourceSurface*>(aMask))
    , mOffset(aOffset)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->MaskSurface(mSource, mMask, mOffset, mOptions);
  }

private:
  AsyncDrawCommandPatternData mSource;
  RefPtr<SourceSurface> mMask;
  Point mOffset;
  DrawOptions mOptions;
};

class AsyncPushClipCommand : public AsyncDrawCommand
{
public:
  explicit AsyncPushClipCommand(const Path* aPath)
    : AsyncDrawCommand(AsyncDrawCommandType::PUSHCLIP)
    , mPath(const_cast<Path*>(aPath))
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->PushClip(mPath);
  }

private:
  RefPtr<Path> mPath;
};

class AsyncPushClipRectCommand : public AsyncDrawCommand
{
public:
  explicit AsyncPushClipRectCommand(const Rect& aRect)
    : AsyncDrawCommand(AsyncDrawCommandType::PUSHCLIPRECT)
    , mRect(aRect)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT)
  {
    aDT->PushClipRect(mRect);
  }

private:
  Rect mRect;
};

class AsyncPopClipCommand : public AsyncDrawCommand
{
public:
  AsyncPopClipCommand()
    : AsyncDrawCommand(AsyncDrawCommandType::POPCLIP)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT)
  {
    aDT->PopClip();
  }
};

class AsyncSetTransformCommand : public AsyncDrawCommand
{
public:
  explicit AsyncSetTransformCommand(const Matrix& aTransform)
    : AsyncDrawCommand(AsyncDrawCommandType::SETTRANSFORM)
    , mTransform(aTransform)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->SetTransform(mTransform);
  }

private:
  Matrix mTransform;
};

class AsyncSetOpaqueRectCommand : public AsyncDrawCommand
{
public:
  explicit AsyncSetOpaqueRectCommand(const IntRect &aRect)
    : AsyncDrawCommand(AsyncDrawCommandType::SETOPAQUERECT)
    , mRect(aRect)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->SetOpaqueRect(mRect);
  }

private:
  IntRect mRect;
};

class AsyncSetPermitSubpixelAACommand : public AsyncDrawCommand
{
public:
  explicit AsyncSetPermitSubpixelAACommand(bool aPermitSubpixelAA)
    : AsyncDrawCommand(AsyncDrawCommandType::SETPERMITSUBPIXELAA)
    , mPermitSubpixelAA(aPermitSubpixelAA)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT) override
  {
    aDT->SetPermitSubpixelAA(mPermitSubpixelAA);
  }

private:
  bool mPermitSubpixelAA;
};

AsyncDrawTarget::AsyncDrawTarget(DrawTarget* aRefDT)
  : mRefDT(aRefDT)
{
  //printf_stderr("bignose create async drawTarget for:%p",aRefDT);

  MOZ_ASSERT(mRefDT);

  mFormat = mRefDT->GetFormat();

  //PL_InitArenaPool(&mPool, "AsyncDrawTarget command", 1024, sizeof(double));
}

AsyncDrawTarget::~AsyncDrawTarget()
{
  //printf_stderr("bignose ~AsyncDrawTarget, addr:%p, data:%p",this,mAsyncDrawTargetData.get());

  //FlushPendingDrawCommand();

  //PL_FinishArenaPool(&mPool);
}

BackendType
AsyncDrawTarget::GetBackendType() const
{
  return mRefDT->GetBackendType();
}

DrawTargetType AsyncDrawTarget::GetType() const
{
  return mRefDT->GetType();
}

already_AddRefed<SourceSurface>
AsyncDrawTarget::Snapshot()
{
  FlushPendingDrawCommand();

  return mRefDT->Snapshot();
}

IntSize
AsyncDrawTarget::GetSize()
{
  return mRefDT->GetSize();
}

void
AsyncDrawTarget::DrawSurface(SourceSurface *aSurface,
                             const Rect &aDest,
                             const Rect &aSource,
                             const DrawSurfaceOptions &aSurfOptions,
                             const DrawOptions &aOptions)
{
  {
  ATRACE_NAME("AsyncDrawTarget::DrawSurface::GuaranteePersistance");
  aSurface->GuaranteePersistance();
  }
  AppendCommand(AsyncDrawSurfaceCommand)(aSurface, aDest, aSource, aSurfOptions, aOptions);
}

void
AsyncDrawTarget::DrawFilter(FilterNode *aNode,
                            const Rect &aSourceRect,
                            const Point &aDestPoint,
                            const DrawOptions &aOptions)
{
  // @todo XXX - this won't work properly long term yet due to filternodes not
  // being immutable.
  AppendCommand(AsyncDrawFilterCommand)(aNode, aSourceRect, aDestPoint, aOptions);
}

void
AsyncDrawTarget::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                       const Point &aDest,
                                       const Color &aColor,
                                       const Point &aOffset,
                                       Float aSigma,
                                       CompositionOp aOperator)
{
  {
  ATRACE_NAME("AsyncDrawTarget::DrawSurfaceWithShadow::GuaranteePersistance");
  aSurface->GuaranteePersistance();
  }
  AppendCommand(AsyncDrawSurfaceWithShadowCommand)(aSurface, aDest, aColor, aOffset, aSigma, aOperator);
}

void
AsyncDrawTarget::ClearRect(const Rect &aRect)
{
  AppendCommand(AsyncClearRectCommand)(aRect);
}

void
AsyncDrawTarget::CopyRect(const IntRect &aSourceRect,
                          const IntPoint &aDestination)
{
  AppendCommand(AsyncCopyRectCommand)(aSourceRect, aDestination);
}

void
AsyncDrawTarget::CopySurface(SourceSurface* aSurface,
                             const IntRect& aSourceRect,
                             const IntPoint& aDestination)
{
  {
  ATRACE_NAME("AsyncDrawTarget::CopySurface::GuaranteePersistance");
  aSurface->GuaranteePersistance();
  }
  AppendCommand(AsyncCopySurfaceCommand)(aSurface, aSourceRect, aDestination);
}

void
AsyncDrawTarget::FillRect(const Rect& aRect,
                          const Pattern& aPattern,
                          const DrawOptions& aOptions)
{
  AppendCommand(AsyncFillRectCommand)(aRect, aPattern, aOptions);
}

void
AsyncDrawTarget::StrokeRect(const Rect& aRect,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aOptions)
{
  AppendCommand(AsyncStrokeRectCommand)(aRect, aPattern, aStrokeOptions, aOptions);
}

void
AsyncDrawTarget::StrokeLine(const Point& aStart,
                            const Point& aEnd,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aOptions)
{
  AppendCommand(AsyncStrokeLineCommand)(aStart, aEnd, aPattern, aStrokeOptions, aOptions);
}

void
AsyncDrawTarget::Stroke(const Path* aPath,
                        const Pattern& aPattern,
                        const StrokeOptions& aStrokeOptions,
                        const DrawOptions& aOptions)
{
  AppendCommand(AsyncStrokeCommand)(aPath, aPattern, aStrokeOptions, aOptions);
}

void
AsyncDrawTarget::Fill(const Path* aPath,
                      const Pattern& aPattern,
                      const DrawOptions& aOptions)
{
  AppendCommand(AsyncFillCommand)(aPath, aPattern, aOptions);
}

void
AsyncDrawTarget::FillGlyphs(ScaledFont* aFont,
                            const GlyphBuffer& aBuffer,
                            const Pattern& aPattern,
                            const DrawOptions& aOptions,
                            const GlyphRenderingOptions* aRenderingOptions)
{
  AppendCommand(AsyncFillGlyphsCommand)(aFont, aBuffer, aPattern, aOptions, aRenderingOptions);
}

void
AsyncDrawTarget::Mask(const Pattern &aSource,
                      const Pattern &aMask,
                      const DrawOptions &aOptions)
{
  AppendCommand(AsyncMaskCommand)(aSource, aMask, aOptions);
}

void
AsyncDrawTarget::MaskSurface(const Pattern &aSource,
                             SourceSurface *aMask,
                             Point aOffset,
                             const DrawOptions &aOptions)
{
  {
  ATRACE_NAME("AsyncDrawTarget::MaskSurface::GuaranteePersistance");
  aMask->GuaranteePersistance();
  }
  AppendCommand(AsyncMaskSurfaceCommand)(aSource, aMask, aOffset, aOptions);
}

void
AsyncDrawTarget::PushClip(const Path* aPath)
{
  AppendCommand(AsyncPushClipCommand)(aPath);
}

void
AsyncDrawTarget::PushClipRect(const Rect& aRect)
{
  AppendCommand(AsyncPushClipRectCommand)(aRect);
}

void
AsyncDrawTarget::PopClip()
{
  AppendCommand(AsyncPopClipCommand)();
}

already_AddRefed<SourceSurface>
AsyncDrawTarget::CreateSourceSurfaceFromData(unsigned char *aData,
                                             const IntSize &aSize,
                                             int32_t aStride,
                                             SurfaceFormat aFormat) const
{
  return mRefDT->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
}

already_AddRefed<SourceSurface>
AsyncDrawTarget::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  return mRefDT->OptimizeSourceSurface(aSurface);
}

already_AddRefed<SourceSurface>
AsyncDrawTarget::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  return mRefDT->CreateSourceSurfaceFromNativeSurface(aSurface);
}

already_AddRefed<DrawTarget>
AsyncDrawTarget::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  return mRefDT->CreateSimilarDrawTarget(aSize, aFormat);
}

already_AddRefed<DrawTarget>
AsyncDrawTarget::CreateShadowDrawTarget(const IntSize &aSize,
                                        SurfaceFormat aFormat,
                                        float aSigma) const
{
  return mRefDT->CreateShadowDrawTarget(aSize, aFormat, aSigma);
}

already_AddRefed<PathBuilder>
AsyncDrawTarget::CreatePathBuilder(FillRule aFillRule) const
{
  return mRefDT->CreatePathBuilder(aFillRule);
}

already_AddRefed<GradientStops>
AsyncDrawTarget::CreateGradientStops(GradientStop *aStops,
                                     uint32_t aNumStops,
                                     ExtendMode aExtendMode) const
{
  return mRefDT->CreateGradientStops(aStops, aNumStops, aExtendMode);
}

already_AddRefed<FilterNode>
AsyncDrawTarget::CreateFilter(FilterType aType)
{
  return mRefDT->CreateFilter(aType);
}

void
AsyncDrawTarget::SetTransform(const Matrix& aTransform)
{
  mTransform = aTransform;
  AppendCommand(AsyncSetTransformCommand)(aTransform);
}

void*
AsyncDrawTarget::GetNativeSurface(NativeSurfaceType aType)
{
  return mRefDT->GetNativeSurface(aType);
}

bool
AsyncDrawTarget::IsDualDrawTarget() const
{
  return mRefDT->IsDualDrawTarget();
}

bool
AsyncDrawTarget::IsTiledDrawTarget() const
{
  return mRefDT->IsTiledDrawTarget();
}

bool
AsyncDrawTarget::SupportsRegionClipping() const
{
  return mRefDT->SupportsRegionClipping();
}

void
AsyncDrawTarget::SetOpaqueRect(const IntRect &aRect)
{
  mOpaqueRect = aRect;
  AppendCommand(AsyncSetOpaqueRectCommand)(aRect);
}

void
AsyncDrawTarget::SetPermitSubpixelAA(bool aPermitSubpixelAA)
{
  mPermitSubpixelAA = aPermitSubpixelAA;
  AppendCommand(AsyncSetPermitSubpixelAACommand)(aPermitSubpixelAA);
}

#ifdef USE_SKIA_GPU
bool
AsyncDrawTarget::InitWithGrContext(GrContext* aGrContext,
                                   const IntSize &aSize,
                                   SurfaceFormat aFormat)
{
  return mRefDT->InitWithGrContext(aGrContext, aSize, aFormat);
}
#endif

void
AsyncDrawTarget::FlushPendingDrawCommand()
{
//  PROFILER_LABEL("AsyncDrawTarget", "FlushPendingDrawCommand",
//    js::ProfileEntry::Category::GRAPHICS);
  ATRACE_NAME("AsyncDrawTarget::FlushPendingDrawCommand");

  //printf_stderr("bignose asyncDrawTarget FlushPendingDrawCommand for:%p",mRefDT.get());

  auto num = mAsyncDrawTargetData->mPendingDrawCommand.size();
  for (decltype(num) i = 0 ; i<num ; ++i) {
    //printf_stderr("bignose asyncDrawTarget exec :%d", mAsyncDrawTargetData->mPendingDrawCommand[i]->GetType());
    mAsyncDrawTargetData->mPendingDrawCommand[i]->ExecuteOnDT(mRefDT.get());
  }

  mAsyncDrawTargetData->DiscardAllDrawCommand();
}

AsyncDrawTargetManager::AsyncDrawTargetManager()
{

}
AsyncDrawTargetManager::~AsyncDrawTargetManager()
{

}

void
AsyncDrawTargetManager::AppendAsyncDrawTarget(AsyncDrawTargetData * aAsyncDrawTarget)
{
  mAsyncDrawTargets.push_back(aAsyncDrawTarget);
}

void
AsyncDrawTargetManager::FlushPendingDrawCommand()
{
  ATRACE_NAME("AsyncDrawTargetManager::FlushPendingDrawCommand");
  //printf_stderr("bignose AsyncDrawTargetManager::FlushPendingDrawCommand");

  auto num = mAsyncDrawTargets.size();

  for (decltype(num) i = 0 ; i<num ; ++i) {
    mAsyncDrawTargets[i]->ApplyDrawCommand();
  }
  mAsyncDrawTargets.clear();
}

AsyncDrawTargetData::AsyncDrawTargetData()
{
  ATRACE_NAME("AsyncDrawTargetData::AsyncDrawTargetData");

  PL_InitArenaPool(&mPool, "AsyncDrawTarget command", 1024, sizeof(double));
}

AsyncDrawTargetData::~AsyncDrawTargetData()
{
  ATRACE_NAME("AsyncDrawTargetData::~AsyncDrawTargetData");
  //printf_stderr("bignose ~AsyncDrawTargetData, addr:%p",this);

  DiscardAllDrawCommand();

  PL_FinishArenaPool(&mPool);
}

void
AsyncDrawTargetData::DiscardAllDrawCommand()
{
  ATRACE_NAME("AsyncDrawTargetData::DiscardAllDrawCommand");

  auto num = mPendingDrawCommand.size();
  for (decltype(num) i = 0 ; i<num ; ++i) {
    mPendingDrawCommand[i]->AsyncDrawCommand::~AsyncDrawCommand();
  }
  mPendingDrawCommand.clear();
  PL_FreeArenaPool(&mPool);
}

void
AsyncDrawTargetData::ApplyDrawCommand()
{
  {
  ATRACE_NAME("AsyncDrawTarget::Lock");

  Lock();
  }

  DrawTarget* drawTarget = GetDrawTarget();
  //printf_stderr("bignose exec draw for dt:%p",drawTarget);

  RefPtr<AsyncDrawTarget> asyncDT = new AsyncDrawTarget(drawTarget);
  asyncDT->SetAsyncDrawTargetData(this);
  asyncDT->FlushPendingDrawCommand();

  {
  ATRACE_NAME("AsyncDrawTarget::Unlock");

  Unlock();
  }
}

} // namespace gfx
} // namespace mozilla
