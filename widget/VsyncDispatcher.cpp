/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/layers/CompositorParent.h"
#include "gfxPrefs.h"
#include "gfxPlatform.h"

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "GeckoProfiler.h"
#include "ProfilerMarkers.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "GeckoTouchDispatcher.h"
#endif

using namespace mozilla::layers;

namespace mozilla {



VsyncDispatcher::VsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  gfxPlatform::GetPlatform()->GetHardwareVsync()->AddVsyncDispatcher(this);
}

VsyncDispatcher::~VsyncDispatcher()
{
  // We auto remove this vsync dispatcher from the vsync source in the nsBaseWidget
}

void
VsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
#ifdef MOZ_ENABLE_PROFILER_SPS
    if (profiler_is_active()) {
        CompositorParent::PostInsertVsyncProfilerMarker(aVsyncTimestamp);
    }
#endif

  if (gfxPrefs::VsyncAlignedCompositor() && mCompositorVsyncObserver) {
    mCompositorVsyncObserver->NotifyVsync(aVsyncTimestamp);
  } else {
    DispatchTouchEvents(aVsyncTimestamp);
  }
}

void
VsyncDispatcher::AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  MOZ_ASSERT(mCompositorVsyncObserver == nullptr);
  mCompositorVsyncObserver = aVsyncObserver;
}

void
VsyncDispatcher::RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread() || NS_IsMainThread());
  MOZ_ASSERT(mCompositorVsyncObserver != nullptr);
  MOZ_ASSERT(mCompositorVsyncObserver == aVsyncObserver);
  mCompositorVsyncObserver = nullptr;
}

void
VsyncDispatcher::Shutdown()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  gfxPlatform::GetPlatform()->GetHardwareVsync()->RemoveVsyncDispatcher(this);
  mCompositorVsyncObserver = nullptr;
}

void
VsyncDispatcher::DispatchTouchEvents(TimeStamp aVsyncTime)
{
  // Touch events can sometimes start a composite, so make sure we dispatch touches
  // even if we don't composite
#ifdef MOZ_WIDGET_GONK
  if (gfxPrefs::TouchResampling()) {
    GeckoTouchDispatcher::NotifyVsync(aVsyncTime);
  }
#endif
}

} // namespace mozilla
