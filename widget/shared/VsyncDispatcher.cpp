/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "nsXULAppAPI.h"
#include "VsyncDispatcherClientImpl.h"
#include "VsyncDispatcherHostImpl.h"

namespace mozilla {

uint32_t
VsyncEventRegistry::GetObserverNum(void) const
{
  return mObserverListList.Length();
}

/*static*/ VsyncDispatcher*
VsyncDispatcher::GetInstance()
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    return VsyncDispatcherHostImpl::GetInstance();
  } else {
    return VsyncDispatcherClientImpl::GetInstance();
  }
}

VsyncEventRegistry*
VsyncDispatcher::GetRefreshDriverRegistry()
{
  MOZ_ASSERT(false, "GetRefreshDriverRegistry should be implemented");

  return nullptr;
}

VsyncEventRegistry*
VsyncDispatcher::GetCompositorRegistry()
{
  MOZ_ASSERT(false, "GetCompositorRegistry should be implemented");

  return nullptr;
}

VsyncDispatcherClient*
VsyncDispatcher::AsVsyncDispatcherClient()
{
  MOZ_ASSERT(false, "Function should be implemented");

  return nullptr;
}

VsyncDispatcherHost*
VsyncDispatcher::AsVsyncDispatcherHost()
{
  MOZ_ASSERT(false, "Function should be implemented");

  return nullptr;
}

} // namespace mozilla
