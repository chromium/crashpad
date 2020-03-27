// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/ios/exception_processor.h"

#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <objc/objc-exception.h>

#include <type_traits>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

namespace crashpad {

// From objc4-779.1/runtime/objc-exception.mm
typedef int _Unwind_Action;
enum : _Unwind_Action {
  _UA_SEARCH_PHASE = 1,
  _UA_CLEANUP_PHASE = 2,
  _UA_HANDLER_FRAME = 4,
  _UA_FORCE_UNWIND = 8
};
typedef int _Unwind_Reason_Code;
enum : _Unwind_Reason_Code {
  _URC_NO_REASON = 0,
  _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
  _URC_FATAL_PHASE2_ERROR = 2,
  _URC_FATAL_PHASE1_ERROR = 3,
  _URC_NORMAL_STOP = 4,
  _URC_END_OF_STACK = 5,
  _URC_HANDLER_FOUND = 6,
  _URC_INSTALL_CONTEXT = 7,
  _URC_CONTINUE_UNWIND = 8
};
struct objc_typeinfo {
  // Position of vtable and name fields must match C++ typeinfo object
  const void** vtable;  // objc_ehtype_vtable+2
  const char* name;  // c++ typeinfo string

  Class cls_unremapped;
};
struct objc_exception {
  id obj;
  struct objc_typeinfo tinfo;
};
OBJC_EXTERN void* __cxa_allocate_exception(size_t thrown_size);

// From libunwind-35.4/include/unwind.h
typedef _Unwind_Reason_Code (*__personality_routine)(
    int version,
    _Unwind_Action actions,
    uint64_t exceptionClass,
    struct _Unwind_Exception* exceptionObject,
    struct _Unwind_Context* context);

static objc_exception_preprocessor g_next_preprocessor = nullptr;

static const char* const kExceptionSinkholes[] = {
    "CFRunLoopRunSpecific",
    "_CFXNotificationPost",
    "__CFRunLoopDoTimers",
    "__CFRunLoopRun",
    "__NSFireDelayedPerform",
    "_dispatch_client_callout",
};

// This function is used to make it clear to the crash processor that this is
// a forced exception crash.
static NOINLINE void TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(id exception) {
  NSString* exception_message_ns = [NSString
      stringWithFormat:@"%@: %@", [exception name], [exception reason]];
  std::string exception_message = base::SysNSStringToUTF8(exception_message_ns);
  //
  //    static crash_reporter::CrashKeyString<256> crash_key("nsexception");
  //    crash_key.Set(exception_message);

  LOG(FATAL) << "Terminating from Objective-C exception: " << exception_message;
}
static id ObjcExceptionPreprocessor(id exception) {
  // Unwind the stack looking for any exception handlers. If an exception
  // handler is encountered, test to see if it is a function known to catch-
  // and-rethrow as a "top-level" exception handler. Various routines in
  // Cocoa do this, and it obscures the crashing stack, since the original
  // throw location is no longer present on the stack (just the re-throw) when
  // Crashpad captures the crash report.
  unw_context_t context;
  unw_getcontext(&context);

  unw_cursor_t cursor;
  unw_init_local(&cursor, &context);

  // Get the base address for the image that contains this function.
  Dl_info dl_info;
  const void* this_base_address = 0;
  if (dladdr(reinterpret_cast<const void*>(&ObjcExceptionPreprocessor),
             &dl_info) != 0) {
    this_base_address = dl_info.dli_fbase;
  }

  // THIS IS WRONG.
  struct objc_exception* exc = (struct objc_exception*)__cxa_allocate_exception(
      sizeof(struct objc_exception));

  while (unw_step(&cursor) > 0) {
    unw_proc_info_t frame_info;
    if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
      continue;
    }

    if (frame_info.handler == 0)
      continue;

    // Check to see if there is really an exception handler.
    __personality_routine p = (__personality_routine)(long)(frame_info.handler);

    // THIS IS WRONG, exception_object->exception_class, exception_object isn't
    // set correctly.
    _Unwind_Reason_Code personalityResult =
        (*p)(1,
             _UA_SEARCH_PHASE,
             0,
             (struct _Unwind_Exception*)exc,
             (struct _Unwind_Context*)(&cursor));
    switch (personalityResult) {
      case _URC_HANDLER_FOUND:
        break;
      case _URC_CONTINUE_UNWIND:
        continue;
      default:
        break;
    }

    char proc_name[64];
    unw_word_t offset;
    if (unw_get_proc_name(&cursor, proc_name, sizeof(proc_name), &offset) !=
        UNW_ESUCCESS) {
      // The symbol has no name, so see if it belongs to the same image as
      // this function.
      if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip),
                 &dl_info) != 0) {
        if (dl_info.dli_fbase == this_base_address) {
          // This is a handler in our image, so allow it to run.
          break;
        }
      }

      // This handler does not belong to us, so continue the search.
      continue;
    }

    // Check if the function is one that is known to obscure (by way of
    // catch-and-rethrow) exception stack traces. If it is, sinkhole it
    // by crashing here at the point of throw.
    for (const char* sinkhole : kExceptionSinkholes) {
      if (strcmp(sinkhole, proc_name) == 0) {
        TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(exception);
      }
    }

    VLOG(1) << "Stopping search for exception handler at " << proc_name;

    break;
  }

  // Forward to the next preprocessor.
  if (g_next_preprocessor)
    return g_next_preprocessor(exception);

  return exception;
}

void InstallObjcExceptionPreprocessor() {
  if (g_next_preprocessor)
    return;
  g_next_preprocessor =
      objc_setExceptionPreprocessor(&ObjcExceptionPreprocessor);
}

void UninstallObjcExceptionPreprocessor() {
  objc_setExceptionPreprocessor(g_next_preprocessor);
  g_next_preprocessor = nullptr;
}

}  // namespace crashpad
