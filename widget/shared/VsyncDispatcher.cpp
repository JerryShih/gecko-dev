/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncDispatcher.h"
#include "VsyncDispatcherClient.h"
#include "VsyncDispatcherHost.h"
#include "nsXULAppAPI.h"

namespace mozilla {

/*static*/ VsyncDispatcher*
VsyncDispatcher::GetInstance()
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    return VsyncDispatcherHost::GetInstance();
  } else {
    return VsyncDispatcherClient::GetInstance();
  }
}

RefreshDriverTrigger*
VsyncDispatcher::AsRefreshDriverTrigger()
{
  return nullptr;
}

CompositorTrigger*
VsyncDispatcher::AsCompositorTrigger()
{
  return nullptr;
}

} // namespace mozilla
