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

// API for capturing source-code location information.
// Based on http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4519.pdf.
//
// To define a function that has access to the source location of the
// callsite, define it with a parameter of type `absl::SourceLocation`. The
// caller can then invoke the function, passing
// `absl::SourceLocation::current()` as the argument.
//
// If at all possible, make the `absl::SourceLocation` parameter be the
// function's last parameter. That way, when `std::source_location` is
// available, you will be able to switch to it, and give the parameter a default
// argument of `std::source_location::current()`. Users will then be able to
// omit that argument, and the default will automatically capture the location
// of the callsite.

#ifndef ABSL_TYPES_SOURCE_LOCATION_H_
#define ABSL_TYPES_SOURCE_LOCATION_H_

#include <cstdint>
#ifdef __has_include
#if __has_include(<source_location>)
#include <source_location>
#endif
#endif

#include "third_party/abseil-cpp/absl/base/config.h"

// TODO(b/134888199): DO NOT OSS: for OSS release, whether to alias to
// std::source_location will need to be configurable via config.h/options.h,
// similar to std::string_view/variant/etc.
#if defined(ABSL_USES_STD_SOURCE_LOCATION) && __cpp_lib_source_location
namespace absl {
ABSL_NAMESPACE_BEGIN
using SourceLocation = std::source_location;
ABSL_NAMESPACE_END
}  // namespace absl

// absl:google3-begin
#elif defined(ABSL_USES_STD_SOURCE_LOCATION) && \
    ABSL_HAVE_BUILTIN(__builtin_source_location)
#include "third_party/absl/types/google/std_source_location_backport.inc"  // IWYU pragma: export
// Note: absl::SourceLocation was defined by std_source_location_backport.inc.
// absl:google3-end
#else  // __cpp_lib_source_location

#if ABSL_HAVE_BUILTIN(__builtin_LINE) && ABSL_HAVE_BUILTIN(__builtin_FILE)
#define ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE 1
#elif defined(__GNUC__)
#define ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE 1
#elif defined(_MSC_VER) && _MSC_VER >= 1926
#define ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE 1
#else
#define ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE 0
#endif

namespace absl {
    ABSL_NAMESPACE_BEGIN

// Class representing a specific location in the source code of a program.
// `absl::SourceLocation` is copyable.
    class SourceLocation {
        struct PrivateTag {
        private:
            explicit PrivateTag() = default;
            friend class SourceLocation;
        };

    public:
        // Avoid this constructor; it populates the object with dummy values.
        constexpr SourceLocation() : line_(0), file_name_("") {}

#if ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE
        // SourceLocation::current
  //
  // Creates a `SourceLocation` based on the current line and file.  APIs that
  // accept a `SourceLocation` as a default parameter can use this to capture
  // their caller's locations.
  //
  // Example:
  //
  //   void TracedAdd(int i, SourceLocation loc = SourceLocation::current()) {
  //     std::cout << loc.file_name() << ":" << loc.line() << " added " << i;
  //     ...
  //   }
  //
  //   void UserCode() {
  //     TracedAdd(1);
  //     TracedAdd(2);
  //   }
  static constexpr SourceLocation current(
      PrivateTag = PrivateTag{}, std::uint_least32_t line = __builtin_LINE(),
      const char* file_name = __builtin_FILE()) {
    return SourceLocation(line, file_name);
  }
#else
        // Creates a dummy `SourceLocation` of "<source_location>" at line number 1,
        // if no `SourceLocation::current()` implementation is available.
        static constexpr SourceLocation current() {
            return SourceLocation(1, "<source_location>");
        }
#endif
        // The line number of the captured source location.
        constexpr std::uint_least32_t line() const { return line_; }

        // The column number of the captured source location. Provided for
        // interface-compatibility with `std::source_location`, but always returns 0
        // in this implementation.
        constexpr std::uint_least32_t column() const { return 0; }

        // The file name of the captured source location. Guaranteed to return a valid
        // pointer to a string constant (not null).
        constexpr const char* file_name() const { return file_name_; }

        // The function name of the captured source location. Provided for
        // interface-compatibility with `std::source_location`, but always returns ""
        // in this implementation.
        constexpr const char* function_name() const { return ""; }

    private:
        // `file_name` must outlive all copies of the `absl::SourceLocation` object,
        // so in practice it should be a string literal.
        constexpr SourceLocation(std::uint_least32_t line, const char* file_name)
                : line_(line),
                  file_name_(file_name) {}

        friend constexpr int UseUnused() {
            static_assert(SourceLocation(0, nullptr).unused_column_ == 0,
                          "Use the otherwise-unused member.");
            return 0;
        }

        // "unused" members are present to minimize future changes in the size of this
        // type.
        std::uint_least32_t line_;
        std::uint_least32_t unused_column_ = 0;
        const char* file_name_;
    };

    ABSL_NAMESPACE_END
}  // namespace absl
#endif  // __cpp_lib_source_location

// Deprecated: just use expansion directly.
#define ABSL_LOC ::absl::SourceLocation::current()

// Deprecated: just use expansion directly.
#define ABSL_LOC_CURRENT_DEFAULT_ARG = ::absl::SourceLocation::current()

#endif  // THIRD_PARTY_ABSL_TYPES_SOURCE_LOCATION_H_