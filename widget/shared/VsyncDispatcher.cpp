/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "nsXULAppAPI.h"
#include "VsyncDispatcherHostImpl.h"

namespace mozilla {

/*static*/ VsyncDispatcher*
VsyncDispatcher::GetInstance()
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    return VsyncDispatcherHostImpl::GetInstance();
  }

  MOZ_RELEASE_ASSERT(true, "Content side should not get VsyncDispatcher");

  return nullptr;
}

VsyncEventRegistry*
VsyncDispatcher::GetInputDispatcherRegistry()
{
  MOZ_ASSERT(false, "GetInputDispatcherRegistry should be implemented");

  return nullptr;
}

VsyncEventRegistry*
VsyncDispatcher::GetCompositorRegistry()
{
  MOZ_ASSERT(false, "GetCompositorRegistry should be implemented");

  return nullptr;
}

} // namespace mozilla
