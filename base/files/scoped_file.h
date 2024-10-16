// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_SCOPED_FILE_H_
#define BASE_FILES_SCOPED_FILE_H_

#include <stdio.h>

#include <memory>
#include <unistd.h>

#include "base/base_export.h"
#include "base/scoped_generic.h"
#include "build/build_config.h"

#if defined(STARBOARD)
#include "starboard/types.h"
#include "starboard/common/file.h"
#endif

namespace base {

namespace internal {

#if defined(STARBOARD)
struct BASE_EXPORT ScopedFDCloseTraits {
  static int InvalidValue() { return -1; }
  static void Free(int file) { if (file >= 0) {close(file);} }
};
#elif BUILDFLAG(IS_ANDROID)
// Use fdsan on android.
struct BASE_EXPORT ScopedFDCloseTraits : public ScopedGenericOwnershipTracking {
  static int InvalidValue() { return -1; }
  static void Free(int);
  static void Acquire(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
  static void Release(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// On ChromeOS and Linux we guard FD lifetime with a global table and hook into
// libc close() to perform checks.
struct BASE_EXPORT ScopedFDCloseTraits : public ScopedGenericOwnershipTracking {
#else
struct BASE_EXPORT ScopedFDCloseTraits {
#endif
  static int InvalidValue() {
    return -1;
  }
  static void Free(int fd);
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  static void Acquire(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
  static void Release(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
#endif
};
#endif

#if defined(STARBOARD)
// Functor for |ScopedFILE| (below).
struct ScopedFILECloser {
  inline void operator()(int* x) const {
    if (x) {
      if (*x >= 0) {
        close(*x);
      }
    }
  }
};
#else
// Functor for |ScopedFILE| (below).
struct ScopedFILECloser {
  inline void operator()(FILE* x) const {
    if (x)
      fclose(x);
  }
};
#endif

}  // namespace internal

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
namespace subtle {

// Enables or disables enforcement of FD ownership as tracked by ScopedFD
// objects. Enforcement is disabled by default since it proves unwieldy in some
// test environments, but tracking is always done. It's best to enable this as
// early as possible in a process's lifetime.
void BASE_EXPORT EnableFDOwnershipEnforcement(bool enabled);

// Resets ownership state of all FDs. The only permissible use of this API is
// in a forked child process between the fork() and a subsequent exec() call.
//
// For one issue, it is common to mass-close most open FDs before calling
// exec(), to avoid leaking FDs into the new executable's environment. For
// processes which have enabled FD ownership enforcement, this reset operation
// is necessary before performing such closures.
//
// Furthermore, fork()+exec() may be used in a multithreaded context, and
// because fork() is not atomic, the FD ownership state in the child process may
// be inconsistent with the actual set of opened file descriptors once fork()
// returns in the child process.
//
// It is therefore especially important to call this ASAP after fork() in the
// child process if any FD manipulation will be done prior to the subsequent
// exec call.
void BASE_EXPORT ResetFDOwnership();

}  // namespace subtle
#endif

// -----------------------------------------------------------------------------

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA) || defined(STARBOARD)
// A low-level Posix file descriptor closer class. Use this when writing
// platform-specific code, especially that does non-file-like things with the
// FD (like sockets).
//
// If you're writing low-level Windows code, see base/win/scoped_handle.h
// which provides some additional functionality.
//
// If you're writing cross-platform code that deals with actual files, you
// should generally use base::File instead which can be constructed with a
// handle, and in addition to handling ownership, has convenient cross-platform
// file manipulation functions on it.
typedef ScopedGeneric<int, internal::ScopedFDCloseTraits> ScopedFD;
#endif

// Automatically closes |FILE*|s.
#if defined(STARBOARD)
typedef std::unique_ptr<starboard::ScopedFile> ScopedFILE;
#else
typedef std::unique_ptr<FILE, internal::ScopedFILECloser> ScopedFILE;
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Queries the ownership status of an FD, i.e. whether it is currently owned by
// a ScopedFD in the calling process.
bool BASE_EXPORT IsFDOwned(int fd);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

}  // namespace base

#endif  // BASE_FILES_SCOPED_FILE_H_
