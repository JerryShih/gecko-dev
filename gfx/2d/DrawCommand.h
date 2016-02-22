/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWCOMMAND_H_
#define MOZILLA_GFX_DRAWCOMMAND_H_

#include <math.h>

#include "2D.h"
#include "Filters.h"
#include "mozilla/UniquePtr.h"

//#define DEBUG_DRAW_COMMAND_TYPE

namespace mozilla {
namespace gfx {

enum class CommandType : int8_t {
  DRAWSURFACE = 0,
  DRAWFILTER,
  DRAWSURFACEWITHSHADOW,
  CLEARRECT,
  COPYSURFACE,
  COPYRECT, //5
  FILLRECT,
  STROKERECT,
  STROKELINE,
  STROKE,
  FILL, //10
  FILLGLYPHS,
  MASK,
  MASKSURFACE,
  PUSHCLIP,
  PUSHCLIPRECT, //15
  POPCLIP,
  PUSHLAYER,
  POPLAYER,
  SETTRANSFORM,
  SETOPAQUERECT, //20
  SETPERMITSUBPIXELAA,
  FLUSH,
};

#ifdef DEBUG_DRAW_COMMAND_TYPE_NAME
#define DRAW_COMMAND_TYPE_NAME(T) \
  virtual const char* GetName() const override { return #T; }
#else
#define DRAW_COMMAND_TYPE_NAME(T)
#endif

class StoredStrokeOptions : public StrokeOptions
{
public:
  explicit StoredStrokeOptions(const StrokeOptions& aStrokeOptions)
    : StrokeOptions(aStrokeOptions.mLineWidth,
                    aStrokeOptions.mLineJoin,
                    aStrokeOptions.mLineCap,
                    aStrokeOptions.mMiterLimit,
                    aStrokeOptions.mDashLength,
                    aStrokeOptions.mDashPattern,
                    aStrokeOptions.mDashOffset)
  {
    if (aStrokeOptions.mDashLength) {
      mDashes.reset(new Float[aStrokeOptions.mDashLength]);
      memcpy(mDashes.get(), aStrokeOptions.mDashPattern, aStrokeOptions.mDashLength * sizeof(Float));
      mDashPattern = mDashes.get();
    }
  }

private:
  UniquePtr<Float> mDashes;
};

class StoredGlyphBuffer : public GlyphBuffer
{
public:
  explicit StoredGlyphBuffer(const GlyphBuffer& aGlyphBuffer)
  {
    mNumGlyphs = aGlyphBuffer.mNumGlyphs;

    if (aGlyphBuffer.mNumGlyphs) {
      mGlyphData.reset(new Glyph[aGlyphBuffer.mNumGlyphs]);
      memcpy(mGlyphData.get(), aGlyphBuffer.mGlyphs, sizeof(Glyph) * aGlyphBuffer.mNumGlyphs);
      mGlyphs = mGlyphData.get();
    }
  }

private:
  UniquePtr<Glyph> mGlyphData;
};


class StoredPattern
{
public:
  explicit StoredPattern(const Pattern& aPattern)
  {
    Assign(aPattern);
  }

  ~StoredPattern()
  {
    reinterpret_cast<Pattern*>(mData.mColor)->~Pattern();
  }

  operator Pattern&()
  {
    return *reinterpret_cast<Pattern*>(mData.mColor);
  }

  operator const Pattern&() const
  {
    return *reinterpret_cast<const Pattern*>(mData.mColor);
  }

  StoredPattern(const StoredPattern& aPattern)
  {
    Assign(aPattern);
  }

  // Block this so that we notice if someone's doing excessive assigning.
  StoredPattern operator=(const StoredPattern& aOther) = delete;

private:
  void Assign(const Pattern& aPattern)
  {
    switch (aPattern.GetType()) {
      case PatternType::COLOR: {
        new (mData.mColor)ColorPattern(*static_cast<const ColorPattern*>(&aPattern));
        break;
      }
      case PatternType::SURFACE: {
        SurfacePattern* surfPat = new (mData.mColor)SurfacePattern(*static_cast<const SurfacePattern*>(&aPattern));
        surfPat->mSurface->GuaranteePersistance();
        break;
      }
      case PatternType::LINEAR_GRADIENT: {
        new (mData.mColor)LinearGradientPattern(*static_cast<const LinearGradientPattern*>(&aPattern));
        break;
      }
      case PatternType::RADIAL_GRADIENT: {
        new (mData.mColor)RadialGradientPattern(*static_cast<const RadialGradientPattern*>(&aPattern));
        break;
      }
      default:
        MOZ_CRASH("Non-support pattern type");
    }
  }

  union PatternData
  {
    char mColor[sizeof(ColorPattern)];
    char mSurface[sizeof(SurfacePattern)];
    char mLinear[sizeof(LinearGradientPattern)];
    char mRadial[sizeof(RadialGradientPattern)];
  };

  PatternData mData;
};

class DrawingCommand
{
public:
  virtual ~DrawingCommand() {}

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aTransform = nullptr) const = 0;

  virtual bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const { return false; }

  CommandType GetType() const { return mType; }

#ifdef DEBUG_DRAW_COMMAND_TYPE_NAME
  virtual const char* GetName() const = 0;
#endif

protected:
  explicit DrawingCommand(CommandType aType)
    : mType(aType)
  {
  }

private:
  CommandType mType;
};

class DrawSurfaceCommand : public DrawingCommand
{
public:
  DrawSurfaceCommand(SourceSurface *aSurface,
                     const Rect& aDest,
                     const Rect& aSource,
                     const DrawSurfaceOptions& aSurfOptions,
                     const DrawOptions& aOptions)
    : DrawingCommand(CommandType::DRAWSURFACE)
    , mSurface(aSurface)
    , mDest(aDest)
    , mSource(aSource)
    , mSurfOptions(aSurfOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->DrawSurface(mSurface, mDest, mSource, mSurfOptions, mOptions);
  }

  DRAW_COMMAND_TYPE_NAME(DrawSurfaceCommand);

private:
  RefPtr<SourceSurface> mSurface;
  Rect mDest;
  Rect mSource;
  DrawSurfaceOptions mSurfOptions;
  DrawOptions mOptions;
};

class DrawFilterCommand : public DrawingCommand
{
public:
  DrawFilterCommand(FilterNode* aFilter,
                    const Rect& aSourceRect,
                    const Point& aDestPoint,
                    const DrawOptions& aOptions)
    : DrawingCommand(CommandType::DRAWSURFACE)
    , mFilter(aFilter)
    , mSourceRect(aSourceRect)
    , mDestPoint(aDestPoint)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->DrawFilter(mFilter, mSourceRect, mDestPoint, mOptions);
  }

  DRAW_COMMAND_TYPE_NAME(DrawFilterCommand);

private:
  RefPtr<FilterNode> mFilter;
  Rect mSourceRect;
  Point mDestPoint;
  DrawOptions mOptions;
};

class DrawSurfaceWithShadowCommand : public DrawingCommand
{
public:
  DrawSurfaceWithShadowCommand(SourceSurface *aSurface,
                               const Point &aDest,
                               const Color &aColor,
                               const Point &aOffset,
                               Float aSigma,
                               CompositionOp aOperator)
    : DrawingCommand(CommandType::DRAWSURFACEWITHSHADOW)
    , mSurface(aSurface)
    , mDest(aDest)
    , mColor(aColor)
    , mOffset(aOffset)
    , mSigma(aSigma)
    , mOperator(aOperator)
  {

  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->DrawSurfaceWithShadow(mSurface, mDest, mColor, mOffset, mSigma, mOperator);
  }

  DRAW_COMMAND_TYPE_NAME(DRAWSURFACEWITHSHADOW);

private:
  RefPtr<SourceSurface> mSurface;
  Point mDest;
  Color mColor;
  Point mOffset;
  Float mSigma;
  CompositionOp mOperator;
};

class ClearRectCommand : public DrawingCommand
{
public:
  explicit ClearRectCommand(const Rect& aRect)
    : DrawingCommand(CommandType::CLEARRECT)
    , mRect(aRect)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->ClearRect(mRect);
  }

  DRAW_COMMAND_TYPE_NAME(ClearRectCommand);

private:
  Rect mRect;
};

class CopyRectCommand : public DrawingCommand
{
public:
  CopyRectCommand(const IntRect &aSourceRect, const IntPoint &aDestination)
    : DrawingCommand(CommandType::COPYRECT)
    , mSourceRect(aSourceRect)
    , mDestination(aDestination)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->CopyRect(mSourceRect, mDestination);
  }

  DRAW_COMMAND_TYPE_NAME(COPYRECT);

private:
  IntRect mSourceRect;
  IntPoint mDestination;
};

class CopySurfaceCommand : public DrawingCommand
{
public:
  CopySurfaceCommand(SourceSurface* aSurface,
                     const IntRect& aSourceRect,
                     const IntPoint& aDestination)
    : DrawingCommand(CommandType::COPYSURFACE)
    , mSurface(aSurface)
    , mSourceRect(aSourceRect)
    , mDestination(aDestination)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aTransform) const
  {
    MOZ_ASSERT(!aTransform || !aTransform->HasNonIntegerTranslation());
    Point dest(Float(mDestination.x), Float(mDestination.y));
    if (aTransform) {
      dest = (*aTransform) * dest;
    }
    aDT->CopySurface(mSurface, mSourceRect, IntPoint(uint32_t(dest.x), uint32_t(dest.y)));
  }

  DRAW_COMMAND_TYPE_NAME(CopySurfaceCommand);

private:
  RefPtr<SourceSurface> mSurface;
  IntRect mSourceRect;
  IntPoint mDestination;
};

class FillRectCommand : public DrawingCommand
{
public:
  FillRectCommand(const Rect& aRect,
                  const Pattern& aPattern,
                  const DrawOptions& aOptions)
    : DrawingCommand(CommandType::FILLRECT)
    , mRect(aRect)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->FillRect(mRect, mPattern, mOptions);
  }

  virtual bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const override
  {
    aDeviceRect = aTransform.TransformBounds(mRect);
    return true;
  }

  DRAW_COMMAND_TYPE_NAME(FillRectCommand);

private:
  Rect mRect;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class StrokeRectCommand : public DrawingCommand
{
public:
  StrokeRectCommand(const Rect& aRect,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions)
    : DrawingCommand(CommandType::STROKERECT)
    , mRect(aRect)
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->StrokeRect(mRect, mPattern, mStrokeOptions, mOptions);
  }

  DRAW_COMMAND_TYPE_NAME(StrokeRectCommand);

private:
  Rect mRect;
  StoredPattern mPattern;
  StoredStrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class StrokeLineCommand : public DrawingCommand
{
public:
  StrokeLineCommand(const Point& aStart,
                    const Point& aEnd,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions)
    : DrawingCommand(CommandType::STROKELINE)
    , mStart(aStart)
    , mEnd(aEnd)
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->StrokeLine(mStart, mEnd, mPattern, mStrokeOptions, mOptions);
  }

  DRAW_COMMAND_TYPE_NAME(StrokeLineCommand);

private:
  Point mStart;
  Point mEnd;
  StoredPattern mPattern;
  StoredStrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class FillCommand : public DrawingCommand
{
public:
  FillCommand(const Path* aPath,
              const Pattern& aPattern,
              const DrawOptions& aOptions)
    : DrawingCommand(CommandType::FILL)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Fill(mPath, mPattern, mOptions);
  }

  virtual bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const override
  {
    aDeviceRect = mPath->GetBounds(aTransform);
    return true;
  }

  DRAW_COMMAND_TYPE_NAME(FillCommand);

private:
  RefPtr<Path> mPath;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.707106781186547524400844362104849039
#endif

// The logic for this comes from _cairo_stroke_style_max_distance_from_path
static Rect
PathExtentsToMaxStrokeExtents(const StrokeOptions &aStrokeOptions,
                              const Rect &aRect,
                              const Matrix &aTransform)
{
  double styleExpansionFactor = 0.5f;

  if (aStrokeOptions.mLineCap == CapStyle::SQUARE) {
    styleExpansionFactor = M_SQRT1_2;
  }

  if (aStrokeOptions.mLineJoin == JoinStyle::MITER &&
      styleExpansionFactor < M_SQRT2 * aStrokeOptions.mMiterLimit) {
    styleExpansionFactor = M_SQRT2 * aStrokeOptions.mMiterLimit;
  }

  styleExpansionFactor *= aStrokeOptions.mLineWidth;

  double dx = styleExpansionFactor * hypot(aTransform._11, aTransform._21);
  double dy = styleExpansionFactor * hypot(aTransform._22, aTransform._12);

  Rect result = aRect;
  result.Inflate(dx, dy);
  return result;
}

class StrokeCommand : public DrawingCommand
{
public:
  StrokeCommand(const Path* aPath,
                const Pattern& aPattern,
                const StrokeOptions& aStrokeOptions,
                const DrawOptions& aOptions)
    : DrawingCommand(CommandType::STROKE)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Stroke(mPath, mPattern, mStrokeOptions, mOptions);
  }

  virtual bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const override
  {
    aDeviceRect = PathExtentsToMaxStrokeExtents(mStrokeOptions, mPath->GetBounds(aTransform), aTransform);
    return true;
  }

  DRAW_COMMAND_TYPE_NAME(StrokeCommand);

private:
  RefPtr<Path> mPath;
  StoredPattern mPattern;
  StoredStrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class FillGlyphsCommand : public DrawingCommand
{
public:
  FillGlyphsCommand(ScaledFont* aFont,
                    const GlyphBuffer& aBuffer,
                    const Pattern& aPattern,
                    const DrawOptions& aOptions,
                    const GlyphRenderingOptions* aRenderingOptions)
    : DrawingCommand(CommandType::FILLGLYPHS)
    , mFont(aFont)
    , mGlyphBuffer(aBuffer)
    , mPattern(aPattern)
    , mOptions(aOptions)
    , mRenderingOptions(const_cast<GlyphRenderingOptions*>(aRenderingOptions))
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->FillGlyphs(mFont, mGlyphBuffer, mPattern, mOptions, mRenderingOptions);
  }

  DRAW_COMMAND_TYPE_NAME(FillGlyphsCommand);

private:
  RefPtr<ScaledFont> mFont;
  StoredGlyphBuffer mGlyphBuffer;
  StoredPattern mPattern;
  DrawOptions mOptions;
  RefPtr<GlyphRenderingOptions> mRenderingOptions;
};

class MaskCommand : public DrawingCommand
{
public:
  MaskCommand(const Pattern& aSource,
              const Pattern& aMask,
              const DrawOptions& aOptions)
    : DrawingCommand(CommandType::MASK)
    , mSource(aSource)
    , mMask(aMask)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Mask(mSource, mMask, mOptions);
  }

  DRAW_COMMAND_TYPE_NAME(MaskCommand);

private:
  StoredPattern mSource;
  StoredPattern mMask;
  DrawOptions mOptions;
};

class MaskSurfaceCommand : public DrawingCommand
{
public:
  MaskSurfaceCommand(const Pattern& aSource,
                     SourceSurface* aMask,
                     const Point& aOffset,
                     const DrawOptions& aOptions)
    : DrawingCommand(CommandType::MASKSURFACE)
    , mSource(aSource)
    , mMask(aMask)
    , mOffset(aOffset)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->MaskSurface(mSource, mMask, mOffset, mOptions);
  }

  DRAW_COMMAND_TYPE_NAME(MaskSurfaceCommand);

private:
  StoredPattern mSource;
  RefPtr<SourceSurface> mMask;
  Point mOffset;
  DrawOptions mOptions;
};

class PushClipCommand : public DrawingCommand
{
public:
  explicit PushClipCommand(const Path* aPath)
    : DrawingCommand(CommandType::PUSHCLIP)
    , mPath(const_cast<Path*>(aPath))
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushClip(mPath);
  }

  DRAW_COMMAND_TYPE_NAME(PushClipCommand);

private:
  RefPtr<Path> mPath;
};

class PushClipRectCommand : public DrawingCommand
{
public:
  explicit PushClipRectCommand(const Rect& aRect)
    : DrawingCommand(CommandType::PUSHCLIPRECT)
    , mRect(aRect)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushClipRect(mRect);
  }

  DRAW_COMMAND_TYPE_NAME(PushClipRectCommand);

private:
  Rect mRect;
};

class PopClipCommand : public DrawingCommand
{
public:
  PopClipCommand()
    : DrawingCommand(CommandType::POPCLIP)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PopClip();
  }

  DRAW_COMMAND_TYPE_NAME(PopClipCommand);
};

class PushLayerCommand : public DrawingCommand
{
public:
  PushLayerCommand(bool aOpaque,
                   Float aOpacity,
                   SourceSurface* aMask,
                   const Matrix& aMaskTransform,
                   const IntRect& aBounds,
                   bool aCopyBackground)
    : DrawingCommand(CommandType::PUSHLAYER)
    , mOpaque(aOpaque)
    , mOpacity(aOpacity)
    , mMask(aMask)
    , mMaskTransform(aMaskTransform)
    , mBounds(aBounds)
    , mCopyBackground(aCopyBackground)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushLayer(mOpaque, mOpacity, mMask, mMaskTransform, mBounds, mCopyBackground);
  }

  DRAW_COMMAND_TYPE_NAME(PushLayerCommand);

private:
  bool mOpaque;
  Float mOpacity;
  RefPtr<SourceSurface> mMask;
  Matrix mMaskTransform;
  IntRect mBounds;
  bool mCopyBackground;
};

class PopLayerCommand : public DrawingCommand
{
public:
  PopLayerCommand()
    : DrawingCommand(CommandType::POPLAYER)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PopLayer();
  }

  DRAW_COMMAND_TYPE_NAME(PopLayerCommand);
};

class SetTransformCommand : public DrawingCommand
{
public:
  explicit SetTransformCommand(const Matrix& aTransform)
    : DrawingCommand(CommandType::SETTRANSFORM)
    , mTransform(aTransform)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aMatrix) const
  {
    if (aMatrix) {
      aDT->SetTransform(mTransform * (*aMatrix));
    } else {
      aDT->SetTransform(mTransform);
    }
  }

  void SetTransform(const Matrix& aMatrix)
  {
    mTransform = aMatrix;
  }

  DRAW_COMMAND_TYPE_NAME(SetTransformCommand);

private:
  Matrix mTransform;
};

class SetOpaqueRectCommand : public DrawingCommand
{
public:
  explicit SetOpaqueRectCommand(const IntRect &aRect)
    : DrawingCommand(CommandType::SETOPAQUERECT)
    , mRect(aRect)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aMatrix) const
  {
    aDT->SetOpaqueRect(mRect);
  }

  DRAW_COMMAND_TYPE_NAME(SetOpaqueRectCommand);

private:
  IntRect mRect;
};

class SetPermitSubpixelAACommand : public DrawingCommand
{
public:
  explicit SetPermitSubpixelAACommand(bool aPermitSubpixelAA)
    : DrawingCommand(CommandType::SETPERMITSUBPIXELAA)
    , mPermitSubpixelAA(aPermitSubpixelAA)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aMatrix) const
  {
    aDT->SetPermitSubpixelAA(mPermitSubpixelAA);
  }

  DRAW_COMMAND_TYPE_NAME(SetPermitSubpixelAACommand);

private:
  bool mPermitSubpixelAA;
};

class FlushCommand : public DrawingCommand
{
public:
  explicit FlushCommand()
    : DrawingCommand(CommandType::FLUSH)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Flush();
  }

  DRAW_COMMAND_TYPE_NAME(FlushCommand);
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWCOMMAND_H_ */
