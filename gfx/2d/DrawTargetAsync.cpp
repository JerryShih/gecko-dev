/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetAsync.h"
#include "DrawCommand.h"
#include "GeckoProfiler.h"
#include "mozilla/dom/UnionMember.h"

#include "MainThreadUtils.h"

//#define PROFILE_SURFACE_GUARANTEE_PERSISTANCE
//#define PROFILE_COMMAND_POOL
//#define PROFILE_DRAW_COMMAND

//#ifndef ATRACE_TAG
//# define ATRACE_TAG ATRACE_TAG_ALWAYS
//#endif
//
//// We need HAVE_ANDROID_OS to be defined for Trace.h.
//// If its not set we will set it temporary and remove it.
//# ifndef HAVE_ANDROID_OS
//#  define HAVE_ANDROID_OS
//#  define REMOVE_HAVE_ANDROID_OS
//# endif
//
//// Android source code will include <cutils/trace.h> before this. There is no
//// HAVE_ANDROID_OS defined in Firefox OS build at that time. Enabled it globally
//// will cause other build break. So atrace_begin and atrace_end are not defined.
//// It will cause a build-break when we include <utils/Trace.h>. Use undef
//// _LIBS_CUTILS_TRACE_H will force <cutils/trace.h> to define atrace_begin and
//// atrace_end with defined HAVE_ANDROID_OS again. Then there is no build-break.
//# undef _LIBS_CUTILS_TRACE_H
//# include <utils/Trace.h>
//# ifdef REMOVE_HAVE_ANDROID_OS
//#  undef HAVE_ANDROID_OS
//#  undef REMOVE_HAVE_ANDROID_OS
//# endif

#define DRAW_COMMAND_TYPENAME(T) \
  virtual const char* GetName() const override { return #T; }

namespace mozilla {
namespace gfx {

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

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) = 0;

  AsyncDrawCommandType GetType() { return mType; }

  virtual const char* GetName() const = 0;

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
        SurfacePattern& surfPattern = mData.mSurface.SetValue(*static_cast<const SurfacePattern*>(&aPattern));
        {
#ifdef PROFILE_SURFACE_GUARANTEE_PERSISTANCE
          //PROFILER_LABEL("AsyncDrawCommandPatternData", "Assign::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
          ATRACE_NAME("AsyncDrawCommandPatternData::Assign::GuaranteePersistance");
#endif
          surfPattern.mSurface->GuaranteePersistance();
        }
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

  DRAW_COMMAND_TYPENAME(DRAWSURFACE);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(DRAWSURFACE);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(DRAWSURFACEWITHSHADOW);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(CLEARRECT);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(COPYRECT);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(COPYSURFACE);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(FILLRECT);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(STROKERECT);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(STROKELINE);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(FILL);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(STROKE);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(FILLGLYPHS);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
  {
    GlyphBuffer buf = { &mGlyphs.front(), (uint32_t)mGlyphs.size()};
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

  DRAW_COMMAND_TYPENAME(MASK);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(MASKSURFACE);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(PUSHCLIP);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(PUSHCLIPRECT);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(POPCLIP);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(SETTRANSFORM);

  void SetTransform(const Matrix& aMatrix)
  {
    mTransform = aMatrix;
  }

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(SETOPAQUERECT);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
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

  DRAW_COMMAND_TYPENAME(SETPERMITSUBPIXELAA);

  virtual void ApplyOnDrawTarget(DrawTarget* aDT) override
  {
    aDT->SetPermitSubpixelAA(mPermitSubpixelAA);
  }

private:
  bool mPermitSubpixelAA;
};

DrawTargetAsync::DrawTargetAsync(AsyncPaintData* aAsyncPaintData)
  : mAsyncPaintData(aAsyncPaintData)
  , mDrawCommandApplied(false)
{
  MOZ_ASSERT(aAsyncPaintData);
  //printf_stderr("bignose create DrawTargetAsync for:%p", mDrawTarget);

  MOZ_ASSERT(Factory::GetAsyncDrawTargetManager());
  Factory::GetAsyncDrawTargetManager()->AppendAsyncPaintData(mAsyncPaintData);

  mFormat = mAsyncPaintData->GetDrawTarget()->GetFormat();
}

DrawTargetAsync::~DrawTargetAsync()
{
  //printf_stderr("bignose ~DrawTargetAsync, addr:%p, dt:%p", this, mDrawTarget);
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
  MOZ_ASSERT(NS_IsMainThread());
  if (mDrawCommandApplied) {
    mDrawCommandApplied = false;
    GetInternalDrawTarget()->Flush();
  }

  //FlushPendingDrawCommand();
  //NS_ASSERTION(0, "DrawTargetAsync::Flush()");
}

void
DrawTargetAsync::ApplyPendingDrawCommand()
{
  mDrawCommandApplied = true;
  mAsyncPaintData->ApplyPendingDrawCommand();

  //NS_ASSERTION(0, "DrawTargetAsync::FlushPendingDrawCommand()");
}

void
DrawTargetAsync::DrawSurface(SourceSurface *aSurface,
                             const Rect &aDest,
                             const Rect &aSource,
                             const DrawSurfaceOptions &aSurfOptions,
                             const DrawOptions &aOptions)
{
  {
#ifdef PROFILE_SURFACE_GUARANTEE_PERSISTANCE
    //PROFILER_LABEL("DrawTargetAsync", "DrawSurface::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
    ATRACE_NAME("DrawTargetAsync::DrawSurface::GuaranteePersistance");
#endif
    aSurface->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<AsyncDrawSurfaceCommand>(aSurface, aDest, aSource, aSurfOptions, aOptions);
}

void
DrawTargetAsync::DrawFilter(FilterNode *aNode,
                            const Rect &aSourceRect,
                            const Point &aDestPoint,
                            const DrawOptions &aOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncDrawFilterCommand>(aNode, aSourceRect, aDestPoint, aOptions);
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
#ifdef PROFILE_SURFACE_GUARANTEE_PERSISTANCE
    //PROFILER_LABEL("DrawTargetAsync", "DrawSurface::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
    ATRACE_NAME("DrawTargetAsync::DrawSurfaceWithShadow::GuaranteePersistance");
#endif
    aSurface->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<AsyncDrawSurfaceWithShadowCommand>(aSurface, aDest, aColor, aOffset, aSigma, aOperator);
}

void
DrawTargetAsync::ClearRect(const Rect &aRect)
{
  mAsyncPaintData->AppendDrawCommand<AsyncClearRectCommand>(aRect);
}

void
DrawTargetAsync::CopyRect(const IntRect &aSourceRect,
                          const IntPoint &aDestination)
{
  mAsyncPaintData->AppendDrawCommand<AsyncCopyRectCommand>(aSourceRect, aDestination);
}

void
DrawTargetAsync::CopySurface(SourceSurface* aSurface,
                             const IntRect& aSourceRect,
                             const IntPoint& aDestination)
{
  {
#ifdef PROFILE_SURFACE_GUARANTEE_PERSISTANCE
    //PROFILER_LABEL("DrawTargetAsync", "CopySurface::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
    ATRACE_NAME("DrawTargetAsync::CopySurface::GuaranteePersistance");
#endif
    aSurface->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<AsyncCopySurfaceCommand>(aSurface, aSourceRect, aDestination);
}

void
DrawTargetAsync::FillRect(const Rect& aRect,
                          const Pattern& aPattern,
                          const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncFillRectCommand>(aRect, aPattern, aOptions);
}

void
DrawTargetAsync::StrokeRect(const Rect& aRect,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncStrokeRectCommand>(aRect, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetAsync::StrokeLine(const Point& aStart,
                            const Point& aEnd,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncStrokeLineCommand>(aStart, aEnd, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetAsync::Stroke(const Path* aPath,
                        const Pattern& aPattern,
                        const StrokeOptions& aStrokeOptions,
                        const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncStrokeCommand>(aPath, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetAsync::Fill(const Path* aPath,
                      const Pattern& aPattern,
                      const DrawOptions& aOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncFillCommand>(aPath, aPattern, aOptions);
}

void
DrawTargetAsync::FillGlyphs(ScaledFont* aFont,
                            const GlyphBuffer& aBuffer,
                            const Pattern& aPattern,
                            const DrawOptions& aOptions,
                            const GlyphRenderingOptions* aRenderingOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncFillGlyphsCommand>(aFont, aBuffer, aPattern, aOptions, aRenderingOptions);
}

void
DrawTargetAsync::Mask(const Pattern& aSource,
                      const Pattern& aMask,
                      const DrawOptions &aOptions)
{
  mAsyncPaintData->AppendDrawCommand<AsyncMaskCommand>(aSource, aMask, aOptions);
}

void
DrawTargetAsync::MaskSurface(const Pattern& aSource,
                             SourceSurface* aMask,
                             Point aOffset,
                             const DrawOptions &aOptions)
{
  {
#ifdef PROFILE_SURFACE_GUARANTEE_PERSISTANCE
    //PROFILER_LABEL("DrawTargetAsync", "MaskSurface::GuaranteePersistance", js::ProfileEntry::Category::GRAPHICS);
    ATRACE_NAME("DrawTargetAsync::MaskSurface::GuaranteePersistance");
#endif
    aMask->GuaranteePersistance();
  }

  mAsyncPaintData->AppendDrawCommand<AsyncMaskSurfaceCommand>(aSource, aMask, aOffset, aOptions);
}

void
DrawTargetAsync::PushClip(const Path* aPath)
{
  mAsyncPaintData->AppendDrawCommand<AsyncPushClipCommand>(aPath);
}

void
DrawTargetAsync::PushClipRect(const Rect& aRect)
{
  mAsyncPaintData->AppendDrawCommand<AsyncPushClipRectCommand>(aRect);
}

void
DrawTargetAsync::PopClip()
{
  mAsyncPaintData->AppendDrawCommand<AsyncPopClipCommand>();
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

  if (mAsyncPaintData->mPendingDrawCommand.size() && (mAsyncPaintData->mPendingDrawCommand.back()->GetType() == AsyncDrawCommandType::SETTRANSFORM)) {
    static_cast<AsyncSetTransformCommand*>(mAsyncPaintData->mPendingDrawCommand.back())->SetTransform(aTransform);
    return;
  }

  mAsyncPaintData->AppendDrawCommand<AsyncSetTransformCommand>(aTransform);
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
  mAsyncPaintData->AppendDrawCommand<AsyncSetOpaqueRectCommand>(aRect);
}

void
DrawTargetAsync::SetPermitSubpixelAA(bool aPermitSubpixelAA)
{
  mPermitSubpixelAA = aPermitSubpixelAA;
  mAsyncPaintData->AppendDrawCommand<AsyncSetPermitSubpixelAACommand>(aPermitSubpixelAA);
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

AsyncDrawTargetManager::AsyncDrawTargetManager()
{

}
AsyncDrawTargetManager::~AsyncDrawTargetManager()
{

}

void
AsyncDrawTargetManager::AppendAsyncPaintData(RefPtr<AsyncPaintData>& aAsyncPaintData)
{
  mPendingDrawData.push_back(aAsyncPaintData);
}

void
AsyncDrawTargetManager::ApplyPendingDrawCommand()
{
#ifdef PROFILE_DRAW_COMMAND
  ATRACE_NAME("AsyncDrawTargetManager::ApplyPendingDrawCommand");
#endif

  auto num = mPendingDrawData.size();

  printf_stderr("!!!!bignose AsyncDrawTargetManager::ApplyPendingDrawCommand, target num:%u",num);

  for (decltype(num) i = 0 ; i<num ; ++i) {
    mPendingDrawData[i]->ApplyPendingDrawCommand();
  }
  mPendingDrawData.clear();
}

void
AsyncDrawTargetManager::ClearResource()
{
  //ATRACE_NAME("AsyncDrawTargetManager::ClearResource");
}

AsyncPaintData::AsyncPaintData()
{
}

AsyncPaintData::~AsyncPaintData()
{
  //printf_stderr("bignose ~AsyncPaintData, addr:%p",this);
  ReleasePool();
}

void
AsyncPaintData::InitPool()
{
  if (mIsPoolReady) {
    return;
  }

#ifdef PROFILE_COMMAND_POOL
  //PROFILER_LABEL("AsyncPaintData", "PL_InitArenaPool", js::ProfileEntry::Category::GRAPHICS);
  ATRACE_NAME("AsyncPaintData::PL_InitArenaPool");
#endif

  const int32_t cmdNum = 256;
  PL_InitArenaPool(&mPool, "AsyncDrawTarget command", cmdNum, sizeof(double));
  mIsPoolReady = true;
}

void
AsyncPaintData::ClearPoolDrawCommand()
{
  if (!mIsPoolReady) {
    return;
  }

  {
#ifdef PROFILE_COMMAND_POOL
    //PROFILER_LABEL("AsyncPaintData", "ClearPoolDrawCommand::~AsyncDrawCommand", js::ProfileEntry::Category::GRAPHICS);
    ATRACE_NAME("AsyncPaintData::ClearPoolDrawCommand::~AsyncDrawCommand");
#endif
    auto num = mPendingDrawCommand.size();
    for (decltype(num) i = 0 ; i<num ; ++i) {
      mPendingDrawCommand[i]->AsyncDrawCommand::~AsyncDrawCommand();
    }
    mPendingDrawCommand.clear();
  }

#ifdef PROFILE_COMMAND_POOL
    //PROFILER_LABEL("AsyncPaintData", "PL_FreeArenaPool", js::ProfileEntry::Category::GRAPHICS);
    ATRACE_NAME("AsyncPaintData::PL_FreeArenaPool");
#endif
  PL_FreeArenaPool(&mPool);
}

void
AsyncPaintData::ReleasePool()
{
  if (!mIsPoolReady) {
    return;
  }

  ClearPoolDrawCommand();

#ifdef PROFILE_COMMAND_POOL
  //PROFILER_LABEL("AsyncPaintData", "PL_FinishArenaPool", js::ProfileEntry::Category::GRAPHICS);
  ATRACE_NAME("AsyncPaintData::PL_FinishArenaPool");
#endif
  PL_FinishArenaPool(&mPool);
}

void
AsyncPaintData::TestDrawCommand()
{
  //ApplyPendingDrawCommand();
}

void
AsyncPaintData::ApplyPendingDrawCommand()
{
#ifdef PROFILE_DRAW_COMMAND
  PROFILER_LABEL("AsyncPaintData", "ApplyPendingDrawCommand", js::ProfileEntry::Category::GRAPHICS);
  ATRACE_NAME("AsyncPaintData::ApplyPendingDrawCommand");
#endif

  bool DEBUG_MSG = false;
  if (NS_IsMainThread()) {
    DEBUG_MSG = true;
  }

  auto commandNum = mPendingDrawCommand.size();

  if (!commandNum) {
    return;
  }

//  if (commandNum>30) {
//    return;
//  }

  LockForAsyncPainting();

  DrawTarget* drawTarget = GetDrawTargetForAsyncPainting();
  MOZ_ASSERT(drawTarget);

  if (DEBUG_MSG)
    printf_stderr("bignose exec draw for dt:%p",drawTarget);

  MOZ_ASSERT(drawTarget);

  if (DEBUG_MSG)
    printf_stderr("bignose AsyncPaintData ApplyPendingDrawCommand for:%p begin",drawTarget);

  for (decltype(commandNum) i = 0 ; i < commandNum ; ++i) {
    if (DEBUG_MSG)
      printf_stderr("bignose AsyncPaintData dt:%p exec op:%s", drawTarget, mPendingDrawCommand[i]->GetName());

    mPendingDrawCommand[i]->ApplyOnDrawTarget(drawTarget);
  }
  ClearPoolDrawCommand();

  if (DEBUG_MSG)
    printf_stderr("bignose AsyncPaintData ApplyPendingDrawCommand for:%p end",drawTarget);

  UnlockForAsyncPainting();
}

} // namespace gfx
} // namespace mozilla
