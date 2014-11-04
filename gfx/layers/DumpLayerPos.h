/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DumpLayerPos_H
#define GFX_DumpLayerPos_H

#include <string>
#include <cmath>

#include "Layers.h"
#include "mozilla/VsyncDispatcherTrace.h"
#include "nsDebug.h"

namespace mozilla {
namespace layers {

class DumpLayer {
public:
  static void DumpPosInfo(Layer* aLayer)
  {
    bool needDumpPos = false;
    std::string className;

    aLayer->GetPosDumpInfo(needDumpPos, className);
    if (needDumpPos){
      gfx::Matrix4x4 transform = aLayer->AsLayerComposite()->GetShadowTransform();
      if (!transform.Is2D()) {
        return;
      }

      gfx::Point pos = transform.As2D().GetTranslation();
      DumpPosDiff(pos);

      bool showPos = false;
      if (showPos) {
        //print to adb log or to gecko profiler
        printf_stderr("silk: layer pos: addr:(%p) pos:(%.2f, %.2f), class name:(%s)",
            aLayer, (float)pos.x, (float)pos.y, className.c_str());
      }
    }
  }

private:
  static void DumpPosDiff(const gfx::Point& aPos)
  {
    gfx::Point nowPos = aPos;

    const int mod = 1024;
    static DataStatistician<float, mod, false> ds("Silk LayerPos", nullptr, nullptr);
    static gfx::Point prevPos = nowPos;

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
        printf_stderr("Silk Layer pos: %llu: (%.1f, %.1f), (dis: %.1f) corner",fn-1, nowPos.x, nowPos.y, distance);
      }
    }

    // dump data
    ds.Update(fn, distance, corner);
    if(!(fn % mod)){
      ds.PrintRawData();
      ds.Reset();
    }
    prevPos = nowPos;
  }

};

} // layers
} // mozilla

#endif // GFX_DumpLayerPos_H
