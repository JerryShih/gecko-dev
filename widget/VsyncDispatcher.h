/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VsyncDispatcher_h
#define mozilla_VsyncDispatcher_h

namespace mozilla {

/*
 * We would like to do some tasks aligned with vsync event. People can implement
 * this class to do this stuff.
 */
class VsyncDispatcher
{
protected:
  virtual ~VsyncDispatcher()
  {
  }

public:
  // Notify vsync event to observers
  virtual void NotifyVsync(int64_t aTimestamp) = 0;
};

} // namespace mozilla

#endif // mozilla_VsyncDispatcher_h
