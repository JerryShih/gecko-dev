/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorLayerComposite.h"
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxPrefs.h"                   // for Preferences
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticFlags::COLOR
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect

// Debug
#include "cmath"
#include "cutils/properties.h"
#include "mozilla/VsyncDispatcherTrace.h"

namespace mozilla {
namespace layers {

void
ColorLayerComposite::RenderLayer(const nsIntRect& aClipRect)
{
  EffectChain effects(this);

  GenEffectChain(effects);

  nsIntRect boundRect = GetBounds();

  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(GetMaskLayer(),
                                                          effects);

  gfx::Rect rect(boundRect.x, boundRect.y,
                 boundRect.width, boundRect.height);
  gfx::Rect clipRect(aClipRect.x, aClipRect.y,
                     aClipRect.width, aClipRect.height);

  float opacity = GetEffectiveOpacity();

  AddBlendModeEffect(effects);

  const gfx::Matrix4x4& transform = GetEffectiveTransform();

  if (gfxPrefs::FrameUniformityDebug()) {
    const gfx::Matrix4x4& m = transform;

    bool onlyPos = false;
    if (onlyPos) {
      printf_stderr("Silk ColorLayer pos: (%.1f, %.1f)",m._41, m._42);
    } else {
      struct pos {
        float x;
        float y;
        pos& operator=(const pos& p) {
          x = p.x;
          y = p.y;
          return *this;
        }
      };
      pos nowPos = {m._41, m._42};

      const int mod = 1024;
      static DataStatistician<float, mod, false> ds("Silk ColorLayer", nullptr, nullptr);
      static pos prevPos = nowPos;

      float diffX = nowPos.x - prevPos.x;
      float diffY = nowPos.y - prevPos.y;
      float distance = std::sqrt((float)(diffX*diffX+diffY*diffY));

      // frame number
      static uint64_t fn = 0;
      ++fn;

      bool corner = false;
      {
        // check corner
        bool xsign = std::signbit(diffX);
        bool ysign = std::signbit(diffY);
        static bool prevXsign = xsign;
        static bool prevYsign = ysign;
        if (ysign != prevYsign || xsign != prevXsign){
          corner = true;
          prevXsign = xsign;
          prevYsign = ysign;
          printf_stderr("Silk ColorLayer pos: %llu: (%.1f, %.1f), (dis: %.1f) corner",fn-1, nowPos.x, nowPos.y, distance);
        }
      }

      // dump data
      ds.Update(fn, distance, corner);
      if(!(fn % mod)){
        char propStat[PROPERTY_VALUE_MAX];
        property_get("silk.debug.stat", propStat, "0");
        if (atoi(propStat) == 1) {
          ds.PrintStatisticData();
        } else {
          ds.PrintRawData();
        }
        ds.Reset();
      }
      prevPos = nowPos;
    }
  }

  mCompositor->DrawQuad(rect, clipRect, effects, opacity, transform);
  mCompositor->DrawDiagnostics(DiagnosticFlags::COLOR,
                               rect, clipRect,
                               transform);
}

void
ColorLayerComposite::GenEffectChain(EffectChain& aEffect)
{
  aEffect.mLayerRef = this;
  gfxRGBA color(GetColor());
  aEffect.mPrimaryEffect = new EffectSolidColor(gfx::Color(color.r,
                                                           color.g,
                                                           color.b,
                                                           color.a));
}

} /* layers */
} /* mozilla */
