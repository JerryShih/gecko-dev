// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_DESCRIPTOR_POSIX_H_
#define BASE_FILE_DESCRIPTOR_POSIX_H_

namespace base {

// -----------------------------------------------------------------------------
// We introduct a special structure for file descriptors in order that we are
// able to use template specialisation to special-case their handling.
//
// WARNING: (Chromium only) There are subtleties to consider if serialising
// these objects over IPC. See comments in chrome/common/ipc_message_utils.h
// above the template specialisation for this structure.
// -----------------------------------------------------------------------------
struct FileDescriptor {
  FileDescriptor()
      : fd(-1),
        auto_close(false),
        dup(false) { }

  FileDescriptor(int ifd, bool iauto_close, bool idup = false)
      : fd(ifd),
        auto_close(iauto_close),
        dup(idup) { }

  int fd;
  // If true, this file descriptor should be closed after it has been used. For
  // example an IPC system might interpret this flag as indicating that the
  // file descriptor it has been given should be closed after use.
  // We will ignore auto_close flag if the fd is processed successfully in
  // thread link case.
  bool auto_close;

  // If true, fd will be assigned with the new fd duplicated(call dup()) from
  // the original one. If auto_close is also true, auto_close will be ignored
  // and skip the dup() call. The fd is just transferred from send to receiver.
  // This flag is only used for thread link. In some case, we should use the
  // duplicated fd instead of sharing the same fd. Process link will ignore this
  // flag. It uses SCM_RIGHTS flag to handle this problem.
  bool dup;
};

}  // namespace base

#endif  // BASE_FILE_DESCRIPTOR_POSIX_H_
