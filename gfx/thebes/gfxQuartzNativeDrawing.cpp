/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxQuartzNativeDrawing.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/Helpers.h"
#include "sstream"
#include "gfxUtils.h"

using namespace mozilla::gfx;
using namespace mozilla;

gfxQuartzNativeDrawing::gfxQuartzNativeDrawing(DrawTarget& aDrawTarget,
                                               const Rect& nativeRect)
  : mDrawTarget(&aDrawTarget)
  , mNativeRect(nativeRect)
  , mCGContext(nullptr)
{
}

CGContextRef
gfxQuartzNativeDrawing::BeginNativeDrawing()
{
  NS_ASSERTION(!mCGContext, "BeginNativeDrawing called when drawing already in progress");

  DrawTarget *dt = mDrawTarget;

  printf_stderr("bignose gfxQuartzNativeDrawing::BeginNativeDrawing, orig dt:%p\n",dt);

#ifdef MOZ_OFF_MAIN_PAINTING
  // with off-main-painting, try to prevent the native drawing to the asyncDT.
  if (true) {
#else
  if (dt->IsDualDrawTarget() || dt->IsTiledDrawTarget()) {
#endif
    // We need a DrawTarget that we can get a CGContextRef from:
    Matrix transform = dt->GetTransform();

    mNativeRect = transform.TransformBounds(mNativeRect);
    mNativeRect.RoundOut();
    // Quartz theme drawing often adjusts drawing rects, so make
    // sure our surface is big enough for that.
    mNativeRect.Inflate(5);
    if (mNativeRect.IsEmpty()) {
      return nullptr;
    }

    mTempDrawTarget =
      Factory::CreateDrawTarget(BackendType::SKIA,
                                IntSize::Truncate(mNativeRect.width, mNativeRect.height),
                                SurfaceFormat::B8G8R8A8);

    if (mTempDrawTarget) {
        transform.PostTranslate(-mNativeRect.x, -mNativeRect.y);
        mTempDrawTarget->SetTransform(transform);
    }
    dt = mTempDrawTarget;

    printf_stderr("bignose gfxQuartzNativeDrawing::BeginNativeDrawing, create temp surface: size(%d,%d) addr:%p\n",
        (int32_t)mNativeRect.width, (int32_t)mNativeRect.height, dt);
  }
  if (dt) {
    printf_stderr("bignose gfxQuartzNativeDrawing::BeginNativeDrawing, borrow dt from surface:%p\n",dt);

    mCGContext = mBorrowedContext.Init(dt);
    MOZ_ASSERT(mCGContext);
  }
  return mCGContext;
}

void
gfxQuartzNativeDrawing::EndNativeDrawing()
{
  NS_ASSERTION(mCGContext, "EndNativeDrawing called without BeginNativeDrawing");

  mBorrowedContext.Finish();
  if (mTempDrawTarget) {
    RefPtr<SourceSurface> source = mTempDrawTarget->Snapshot();


    //static int count = 0;
    //++count;

    //std::stringstream sstream;
    //sstream << "/Users/bignose/tmp/img/dt_" << count << '_' <<
    //    source->GetSize().width << '_' << source->GetSize().height << ".png";

    //gfxUtils::WriteAsPNG(source, sstream.str().c_str());

    AutoRestoreTransform autoRestore(mDrawTarget);
    mDrawTarget->SetTransform(Matrix());
    mDrawTarget->DrawSurface(source, mNativeRect,
                             Rect(0, 0, mNativeRect.width, mNativeRect.height));
  }
}
