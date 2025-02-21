//
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

// Some of our malloc implementations can invoke the following hooks whenever
// memory is allocated or deallocated.  MallocHook is thread-safe, and things
// you do before calling AddFooHook(MyHook) are visible to any resulting calls
// to MyHook.  Hooks must be thread-safe.  If you write:
//
//   CHECK(MallocHook::AddNewHook(&MyNewHook));
//
// MyNewHook will be invoked in subsequent calls in the current thread, but
// there are no guarantees on when it might be invoked in other threads.
//
// There are a limited number of slots available for each hook type.  Add*Hook
// will return false if there are no slots available.  Remove*Hook will return
// false if the given hook was not already installed.
//
// The order in which individual hooks are called in Invoke*Hook is undefined.
//
// It is safe for a hook to remove itself within Invoke*Hook and add other
// hooks.  Any hooks added inside a hook invocation (for the same hook type)
// will not be invoked for the current invocation.
//
// One important user of these hooks is the heap profiler.
//
// CAVEAT: If you add new MallocHook::Invoke* calls then those calls must be
// directly in the code of the (de)allocation function that is provided to the
// user and that function must have an ABSL_ATTRIBUTE_SECTION(malloc_hook)
// attribute.
//
// Note: the Invoke*Hook() functions are defined in malloc_hook-inl.h.  If you
// need to invoke a hook (which you shouldn't unless you're part of tcmalloc),
// be sure to #include malloc_hook-inl.h in addition to malloc_hook.h.
//
// IWYU pragma: private, include "base/malloc_hook.h"
// IWYU pragma: friend third_party/absl/.*

#ifndef THIRD_PARTY_ABSL_BASE_INTERNAL_MALLOC_HOOK_H_
#define THIRD_PARTY_ABSL_BASE_INTERNAL_MALLOC_HOOK_H_

#include <sys/types.h>

#include <cstddef>

#include "third_party/base/censushandle.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/abseil-cpp/absl/base/config.h"
#include "third_party/abseil-cpp/absl/base/port.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace absl {
    ABSL_NAMESPACE_BEGIN
    namespace base_internal {

// Enum for recording the section to which the frame with hook belongs to.
        enum class HookSection : int {
            kNone,
            kGoogleMalloc,
            kMallochook,
            kUnionAlloc,
            kBlinkAlloc
        };

// Enum to control how a hook can access the memory.
        enum class HookMemoryMutable : bool {
            kImmutable,
            kMutable,
        };

        class MallocHook {
        public:
            struct NewInfo final {
                // Pointer to the allocated memory.
                void* ptr = nullptr;
                // Requested allocation size.
                size_t requested_size = 0;
                // Actual allocation size, if implemented by the allocator. Defaults to 0.
                size_t allocated_size = 0;
                // Allow a hook to modify the memory.
                HookMemoryMutable is_mutable;
            };
            // The NewHook is invoked whenever an object is being allocated.
            // Object pointer and size are passed in.
            // It may be passed null pointer if the allocator returned null.
            typedef void (*NewHook)(const NewInfo& info);
            static bool AddNewHook(NewHook hook);
            static bool RemoveNewHook(NewHook hook);
            inline static void InvokeNewHook(const void* ptr, size_t requested_size,
                                             size_t allocated_size = 0);
            inline static void InvokeNewHook(const NewInfo& info);

            struct DeleteInfo final {
                // Pointer to the deallocated memory.
                void* ptr = nullptr;
                // Size of the allocated memory.
                size_t allocated_size = 0;
                // Allow a hook to modify the memory.
                HookMemoryMutable is_mutable;
            };

            // The DeleteHook is invoked whenever an object is being deallocated.
            // Object pointer is passed in.
            // It may be passed null pointer if the caller is trying to delete null.
            typedef void (*DeleteHook)(const DeleteInfo& info);
            static bool AddDeleteHook(DeleteHook hook);
            static bool RemoveDeleteHook(DeleteHook hook);
            inline static void InvokeDeleteHook(const void* ptr,
                                                size_t allocated_size = 0);
            inline static void InvokeDeleteHook(const DeleteInfo& info);

            // The SampledNewHook is invoked for some subset of object allocations
            // according to the sampling policy of an allocator such as tcmalloc.
            // SampledAlloc has the following fields:
            //  * AllocHandle handle: to be set to an effectively unique value (in this
            //    process) by allocator.
            //  * size_t allocated_size: space actually used by allocator to host the
            //    object. Not necessarily equal to the requested size due to alignment
            //    and other reasons.
            //  * double weight: the expected number of allocations matching this profile
            //    that this sample represents. This weight does not need to be >= 1.0;
            //    tcmalloc routinely generates weights less than unity (typically in the
            //    case of larger allocations). The value is still expected to be
            //    non-negative.
            //  * int stack_depth and const void* stack: invocation stack for
            //    the allocation.
            //  * CensusHandle census_handle_internal_only: the Census handle present at
            //    the time of the sampled allocation. WARNING: Census team internal use
            //    only, do not read/write.
            //  * const void* ptr: the address of the allocated memory.
            // The allocator invoking the hook has all the fields in `SampledAlloc` stored
            // and later call InvokeSampledDeleteHook() with a `SampledAlloc` struct
            // populated by those fields.
            typedef int64_t AllocHandle;
            struct SampledAlloc {
                const AllocHandle handle;
                const size_t requested_size;
                const size_t requested_alignment;
                const size_t allocated_size;
                const double weight;
                const int stack_depth;
                const void* stack;
                const absl::Time allocation_time;
                CensusHandle census_handle_internal_only;
                const void* ptr;
            };
            typedef void (*SampledNewHook)(const SampledAlloc* sampled_alloc);
            static bool AddSampledNewHook(SampledNewHook hook);
            static bool RemoveSampledNewHook(SampledNewHook hook);
            inline static void InvokeSampledNewHook(const SampledAlloc* sampled_alloc);

            // The SampledDeleteHook is invoked whenever an object previously chosen
            // by an allocator for sampling is being deallocated.
            // A `SampledAlloc` struct identifying the object -- as all its fields have
            // been stored by the allocator -- is passed in.
            typedef void (*SampledDeleteHook)(SampledAlloc* sampled_alloc);
            static bool AddSampledDeleteHook(SampledDeleteHook hook);
            static bool RemoveSampledDeleteHook(SampledDeleteHook hook);
            inline static void InvokeSampledDeleteHook(SampledAlloc* sampled_alloc);

            // The PreMmapHook is invoked with mmap's or mmap64's arguments just
            // before the mmap/mmap64 call is actually made.  Such a hook may be useful
            // in memory limited contexts, to catch allocations that will exceed
            // a memory limit, and take outside actions to increase that limit.
            typedef void (*PreMmapHook)(const void* start, size_t size, int protection,
                                        int flags, int fd, off_t offset);
            static bool AddPreMmapHook(PreMmapHook hook);
            static bool RemovePreMmapHook(PreMmapHook hook);
            inline static void InvokePreMmapHook(const void* start,
                                                 size_t size,
                                                 int protection,
                                                 int flags,
                                                 int fd,
                                                 off_t offset);

            // The MmapReplacement is invoked with mmap's arguments and place to put the
            // result into after the PreMmapHook but before the mmap/mmap64 call is
            // actually made.
            // The MmapReplacement should return true if it handled the call, or false
            // if it is still necessary to call mmap/mmap64.
            // This should be used only by experts, and users must be be
            // extremely careful to avoid recursive calls to mmap. The replacement
            // should be async signal safe.
            // Only one MmapReplacement is supported. After setting an MmapReplacement
            // you must call RemoveMmapReplacement before calling SetMmapReplacement
            // again.
            typedef int (*MmapReplacement)(const void* start, size_t size, int protection,
                                           int flags, int fd, off_t offset,
                                           void** result);
            static bool SetMmapReplacement(MmapReplacement hook);
            static bool RemoveMmapReplacement(MmapReplacement hook);
            inline static bool InvokeMmapReplacement(const void* start,
                                                     size_t size,
                                                     int protection,
                                                     int flags,
                                                     int fd,
                                                     off_t offset,
                                                     void** result);


            // The MmapHook is invoked with mmap's return value and arguments whenever
            // a region of memory has been just mapped.
            // It may be passed MAP_FAILED if the mmap failed.
            typedef void (*MmapHook)(const void* result, const void* start, size_t size,
                                     int protection, int flags, int fd, off_t offset);
            static bool AddMmapHook(MmapHook hook);
            static bool RemoveMmapHook(MmapHook hook);
            inline static void InvokeMmapHook(const void* result,
                                              const void* start,
                                              size_t size,
                                              int protection,
                                              int flags,
                                              int fd,
                                              off_t offset);

            // The MunmapReplacement is invoked with munmap's arguments and place to put
            // the result into just before the munmap call is actually made.
            // The MunmapReplacement should return true if it handled the call, or false
            // if it is still necessary to call munmap.
            // This should be used only by experts. The replacement should be
            // async signal safe.
            // Only one MunmapReplacement is supported. After setting an
            // MunmapReplacement you must call RemoveMunmapReplacement before
            // calling SetMunmapReplacement again.
            typedef int (*MunmapReplacement)(const void* start, size_t size, int* result);
            static bool SetMunmapReplacement(MunmapReplacement hook);
            static bool RemoveMunmapReplacement(MunmapReplacement hook);
            inline static bool InvokeMunmapReplacement(const void* start,
                                                       size_t size,
                                                       int* result);

            // The PreMunmapHook is invoked with munmap's arguments just before the munmap
            // call is actually made.
            // TODO(nilayvaish): maybe this pre-hook is not needed.  Explore moving the
            // clients to the post hook and drop this one.
            typedef void (*PreMunmapHook)(const void* start, size_t size);
            static bool AddPreMunmapHook(PreMunmapHook hook);
            static bool RemovePreMunmapHook(PreMunmapHook hook);
            inline static void InvokePreMunmapHook(const void* start, size_t size);

            // The MunmapHook is invoked with munmap's arguments and result just after the
            // munmap call is actually made.
            typedef void (*MunmapHook)(const void* start, size_t size, int result);
            static bool AddMunmapHook(MunmapHook hook);
            static bool RemoveMunmapHook(MunmapHook hook);
            inline static void InvokeMunmapHook(const void* start, size_t size,
                                                int result);

            // The MremapHook is invoked with mremap's return value and arguments
            // whenever a region of memory has been just remapped.
            typedef void (*MremapHook)(const void* result, const void* old_addr,
                                       size_t old_size, size_t new_size, int flags,
                                       const void* new_addr);
            static bool AddMremapHook(MremapHook hook);
            static bool RemoveMremapHook(MremapHook hook);
            inline static void InvokeMremapHook(const void* result,
                                                const void* old_addr,
                                                size_t old_size,
                                                size_t new_size,
                                                int flags,
                                                const void* new_addr);

            // The PreSbrkHook is invoked with sbrk's argument just before sbrk is called
            // -- except when the increment is 0.  This is because sbrk(0) is often called
            // to get the top of the memory stack, and is not actually a
            // memory-allocation call.  It may be useful in memory-limited contexts,
            // to catch allocations that will exceed the limit and take outside
            // actions to increase such a limit.
            typedef void (*PreSbrkHook)(ptrdiff_t increment);
            static bool AddPreSbrkHook(PreSbrkHook hook);
            static bool RemovePreSbrkHook(PreSbrkHook hook);
            inline static void InvokePreSbrkHook(ptrdiff_t increment);

            // The SbrkHook is invoked with sbrk's result and argument whenever sbrk
            // has just executed -- except when the increment is 0.
            // This is because sbrk(0) is often called to get the top of the memory stack,
            // and is not actually a memory-allocation call.
            typedef void (*SbrkHook)(const void* result, ptrdiff_t increment);
            static bool AddSbrkHook(SbrkHook hook);
            static bool RemoveSbrkHook(SbrkHook hook);
            inline static void InvokeSbrkHook(const void* result, ptrdiff_t increment);

            // Pointer to a absl::GetStackTrace implementation, following the API in
            // base/stacktrace.h.
            using GetStackTraceFn = int (*)(void**, int, int);

            // Get the current stack trace.  Try to skip all routines up to and
            // including the caller of MallocHook::Invoke*.
            // Use "skip_count" (similarly to absl::GetStackTrace from stacktrace.h)
            // as a hint about how many routines to skip if better information
            // is not available.
            // Stack trace is filled into *result up to the size of max_depth.
            // The actual number of stack frames filled is returned.
            static int GetCallerStackTrace(void** result, int max_depth, int skip_count,
                                           GetStackTraceFn get_stack_trace_fn);
            // Mostly similar to the above function, but does not skip functions based on
            // when MallocHook::Invoke was called.  Instead returns the names of the hook
            // sections via the sections array, if not null.  When not null, the size of
            // the sections array should match that of the results array.
            static int GetCallerStackTraceAndSections(void** result,
                                                      HookSection* sections,
                                                      int max_depth, int skip_count,
                                                      GetStackTraceFn get_stack_trace_fn);

#if ABSL_HAVE_MMAP
            // Unhooked versions of mmap() and munmap().   These should be used
  // only by experts, since they bypass heapchecking, etc.
  // Note: These do not run hooks, but they still use the MmapReplacement
  // and MunmapReplacement.
  static void* UnhookedMMap(void* start, size_t size, int protection, int flags,
                            int fd, off_t offset);
  static int UnhookedMUnmap(void* start, size_t size);
#endif

        private:
            // Slow path versions of Invoke*Hook.
            static void InvokeNewHookSlow(const NewInfo& info) ABSL_ATTRIBUTE_COLD;
            static void InvokeDeleteHookSlow(const DeleteInfo& info) ABSL_ATTRIBUTE_COLD;
            static void InvokeSampledNewHookSlow(const SampledAlloc* sampled_alloc)
            ABSL_ATTRIBUTE_COLD;
            static void InvokeSampledDeleteHookSlow(SampledAlloc* sampled_alloc)
            ABSL_ATTRIBUTE_COLD;
            static void InvokePreMmapHookSlow(const void* start, size_t size,
                                              int protection, int flags, int fd,
                                              off_t offset) ABSL_ATTRIBUTE_COLD;
            static void InvokeMmapHookSlow(const void* result, const void* start,
                                           size_t size, int protection, int flags, int fd,
                                           off_t offset) ABSL_ATTRIBUTE_COLD;
            static bool InvokeMmapReplacementSlow(const void* start, size_t size,
                                                  int protection, int flags, int fd,
                                                  off_t offset,
                                                  void** result) ABSL_ATTRIBUTE_COLD;
            static void InvokePreMunmapHookSlow(const void* start,
                                                size_t size) ABSL_ATTRIBUTE_COLD;
            static void InvokeMunmapHookSlow(const void* start, size_t size,
                                             int result) ABSL_ATTRIBUTE_COLD;
            static bool InvokeMunmapReplacementSlow(const void* ptr, size_t size,
                                                    int* result) ABSL_ATTRIBUTE_COLD;
            static void InvokeMremapHookSlow(const void* result, const void* old_addr,
                                             size_t old_size, size_t new_size, int flags,
                                             const void* new_addr) ABSL_ATTRIBUTE_COLD;
            static void InvokePreSbrkHookSlow(ptrdiff_t increment) ABSL_ATTRIBUTE_COLD;
            static void InvokeSbrkHookSlow(const void* result,
                                           ptrdiff_t increment) ABSL_ATTRIBUTE_COLD;
        };

    }  // namespace base_internal
    ABSL_NAMESPACE_END
}  // namespace absl

using absl::base_internal::MallocHook;
using absl::base_internal::HookMemoryMutable;
#endif  // THIRD_PARTY_ABSL_BASE_INTERNAL_MALLOC_HOOK_H_