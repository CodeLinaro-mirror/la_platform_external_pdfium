// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This files is a forwarding header for other headers containing various
// portability macros and functions.
// absl:google3-begin(Internal-only note)
// It also contains obsolete things that are pending cleanup but need to stay
// in Abseil for now.
// absl:google3-end

#ifndef ABSL_BASE_PORT_H_
#define ABSL_BASE_PORT_H_

#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/abseil-cpp/absl/base/config.h"
#include "third_party/abseil-cpp/absl/base/optimization.h"

// absl:google3-begin(Not shipping SWIG, global string, or hashing)
#ifdef SWIG
%include "third_party/absl/base/attributes.h"
#endif

// -----------------------------------------------------------------------------
// Obsolete (to be removed)
// -----------------------------------------------------------------------------

// NOTE: These live in Abseil purely as a short-term layering workaround to
// resolve a dependency chain between util/hash/hash, absl/strings, and //base:
// in order for //base to depend on absl/strings, the includes of hash need
// to be in absl, not //base.  string_view defines hashes.
//
// -----------------------------------------------------------------------------
// HASH_NAMESPACE, HASH_NAMESPACE_DECLARATION_START/END
// -----------------------------------------------------------------------------

// Define the namespace for pre-C++11 functors for hash_map and hash_set.
// This is not the namespace for C++11 functors (that namespace is "std").
//
// We used to require that the build tool or Makefile provide this definition.
// Now we usually get it from testing target macros. If the testing target
// macros are different from an external definition, you will get a build
// error.
//
// TODO(marmstrong): always get HASH_NAMESPACE from testing target macros.

#if defined(_MSC_VER) && !defined(_LIBCPP_VERSION)
// MSVC.
// http://msdn.microsoft.com/en-us/library/6x7w9f6z(v=vs.100).aspx
#define HASH_NAMESPACE stdext
#elif defined(__GNUC__) || defined(__APPLE__) || \
    (defined(_MSC_VER) && defined(_LIBCPP_VERSION))
// Standard GCC-compatible compilation environment, or Xcode.
#define HASH_NAMESPACE __gnu_cxx
#else
// HASH_NAMESPACE defined externally.
// TODO(marmstrong): make this an error. Do not use external value of
// HASH_NAMESPACE.
#endif

#ifndef HASH_NAMESPACE
// TODO(marmstrong): try to delete this.
// I think gcc 2.95.3 was the last toolchain to use this.
#define HASH_NAMESPACE_DECLARATION_START
#define HASH_NAMESPACE_DECLARATION_END
#else
#define HASH_NAMESPACE_DECLARATION_START namespace HASH_NAMESPACE {
#define HASH_NAMESPACE_DECLARATION_END }
#endif

// Significant parts of //net/proto2, //util/hash, etc., assume that `hash`
// exists in the HASH_NAMESPACE. However, in MSVC runtime versions after VS
// 2015, Microsoft removed the directive:
//     using _STD hash;
// from their stdext namespace, meaning that `hash` is no longer present.
// Since this is a change in layout, we temporarily restore this directive while
// we're working on removing this assumption.
// TODO(b/140888989): Remove this once the patch is no longer needed.
#if defined(_MSC_VER) && defined(__cplusplus) && !defined(_LIBCPP_VERSION)
#include <functional>
HASH_NAMESPACE_DECLARATION_START
using _STD hash;
HASH_NAMESPACE_DECLARATION_END
#endif
// absl:google3-end

#endif  // THIRD_PARTY_ABSL_BASE_PORT_H_