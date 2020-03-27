// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/ios/exception_processor.h"

#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <objc/objc-exception.h>

#include <type_traits>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"

namespace crashpad {

// Maximum number of known named exceptions we'll support.  There is
// no central registration, but I only find about 75 possibilities in
// the system frameworks, and many of them are probably not
// interesting to track in aggregate (those relating to distributed
// objects, for instance).
constexpr size_t kKnownNSExceptionCount = 25;

const size_t kUnknownNSException = kKnownNSExceptionCount;

size_t BinForException(NSException* exception) {
  // A list of common known exceptions.  The list position will
  // determine where they live in the histogram, so never move them
  // around, only add to the end.
  NSString* const kKnownNSExceptionNames[] = {
      // Grab-bag exception, not very common.  CFArray (or other
      // container) mutated while being enumerated is one case seen in
      // production.
      NSGenericException,

      // Out-of-range on NSString or NSArray.  Quite common.
      NSRangeException,

      // Invalid arg to method, unrecognized selector.  Quite common.
      NSInvalidArgumentException,

      // malloc() returned null in object creation, I think.  Turns out
      // to be very uncommon in production, because of the OOM killer.
      NSMallocException,

      // This contains things like windowserver errors, trying to draw
      // views which aren't in windows, unable to read nib files.  By
      // far the most common exception seen on the crash server.
      NSInternalInconsistencyException,
  };

  // Make sure our array hasn't outgrown our abilities to track it.
  static_assert(base::size(kKnownNSExceptionNames) < kKnownNSExceptionCount,
                "Cannot track more exceptions");

  NSString* name = [exception name];
  for (size_t i = 0; i < base::size(kKnownNSExceptionNames); ++i) {
    if (name == kKnownNSExceptionNames[i]) {
      return i;
    }
  }
  return kUnknownNSException;
}

static objc_exception_preprocessor g_next_preprocessor = nullptr;

// static NSUncaughtExceptionHandler* g_previous_uncaught_exception_handler =
//    nullptr;

static const char* const kExceptionSinkholes[] = {
    "CFRunLoopRunSpecific",
    "_CFXNotificationPost",
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

  //  static crash_reporter::CrashKeyString<256> crash_key("nsexception");
  //  crash_key.Set(exception_message);

  LOG(FATAL) << "Terminating from Objective-C exception: " << exception_message;
}


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



struct frame_ips {
    uintptr_t start;
    uintptr_t end;
};
struct frame_range {
    uintptr_t ip_start;
    uintptr_t ip_end;
    uintptr_t cfa;
    // precise ranges within ip_start..ip_end; nil or {0,0} terminated
    frame_ips *ips;
};

struct dwarf_eh_bases
{
    uintptr_t tbase;
    uintptr_t dbase;
    uintptr_t func;
};
extern "C" _Unwind_Reason_Code __objc_personality_v0(int, _Unwind_Action, uint64_t, struct _Unwind_Exception*, struct _Unwind_Context*);


#include <libunwind.h>
#include <execinfo.h>
#include <dispatch/dispatch.h>

// Dwarf eh data encodings
#define DW_EH_PE_omit      0xff  // no data follows

#define DW_EH_PE_absptr    0x00
#define DW_EH_PE_uleb128   0x01
#define DW_EH_PE_udata2    0x02
#define DW_EH_PE_udata4    0x03
#define DW_EH_PE_udata8    0x04
#define DW_EH_PE_sleb128   0x09
#define DW_EH_PE_sdata2    0x0A
#define DW_EH_PE_sdata4    0x0B
#define DW_EH_PE_sdata8    0x0C

#define DW_EH_PE_pcrel     0x10
#define DW_EH_PE_textrel   0x20
#define DW_EH_PE_datarel   0x30
#define DW_EH_PE_funcrel   0x40
#define DW_EH_PE_aligned   0x50  // fixme

#define DW_EH_PE_indirect  0x80  // gcc extension


/***********************************************************************
* read_uleb
* Read a LEB-encoded unsigned integer from the address stored in *pp.
* Increments *pp past the bytes read.
* Adapted from DWARF Debugging Information Format 1.1, appendix 4
**********************************************************************/
static uintptr_t read_uleb(uintptr_t *pp)
{
    uintptr_t result = 0;
    uintptr_t shift = 0;
    unsigned char byte;
    do {
        byte = *(const unsigned char *)(*pp)++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}


/***********************************************************************
* read_sleb
* Read a LEB-encoded signed integer from the address stored in *pp.
* Increments *pp past the bytes read.
* Adapted from DWARF Debugging Information Format 1.1, appendix 4
**********************************************************************/
static intptr_t read_sleb(uintptr_t *pp)
{
    uintptr_t result = 0;
    uintptr_t shift = 0;
    unsigned char byte;
    do {
        byte = *(const unsigned char *)(*pp)++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    if ((shift < 8*sizeof(intptr_t))  &&  (byte & 0x40)) {
        result |= ((intptr_t)-1) << shift;
    }
    return result;
}


/***********************************************************************
* read_address
* Reads an encoded address from the address stored in *pp.
* Increments *pp past the bytes read.
* The data is interpreted according to the given dwarf encoding
* and base addresses.
**********************************************************************/
static uintptr_t read_address(uintptr_t *pp,
                              const struct dwarf_eh_bases *bases,
                              unsigned char encoding)
{
    uintptr_t result = 0;
    uintptr_t oldp = *pp;

    // fixme need DW_EH_PE_aligned?

#define READ(type) \
    result = *(type *)(*pp); \
    *pp += sizeof(type);

    if (encoding == DW_EH_PE_omit) return 0;

    switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
        READ(uintptr_t);
        break;
    case DW_EH_PE_uleb128:
        result = read_uleb(pp);
        break;
    case DW_EH_PE_udata2:
        READ(uint16_t);
        break;
    case DW_EH_PE_udata4:
        READ(uint32_t);
        break;
#if __LP64__
    case DW_EH_PE_udata8:
        READ(uint64_t);
        break;
#endif
    case DW_EH_PE_sleb128:
        result = read_sleb(pp);
        break;
    case DW_EH_PE_sdata2:
        READ(int16_t);
        break;
    case DW_EH_PE_sdata4:
        READ(int32_t);
        break;
#if __LP64__
    case DW_EH_PE_sdata8:
        READ(int64_t);
        break;
#endif
    default:
//        _objc_inform("unknown DWARF EH encoding 0x%x at %p",
//                     encoding, (void *)*pp);
        break;
    }

#undef READ

    if (result) {
        switch (encoding & 0x70) {
        case DW_EH_PE_pcrel:
            // fixme correct?
            result += (uintptr_t)oldp;
            break;
        case DW_EH_PE_textrel:
            result += bases->tbase;
            break;
        case DW_EH_PE_datarel:
            result += bases->dbase;
            break;
        case DW_EH_PE_funcrel:
            result += bases->func;
            break;
        case DW_EH_PE_aligned:
//            _objc_inform("unknown DWARF EH encoding 0x%x at %p",
//                         encoding, (void *)*pp);
            break;
        default:
            // no adjustment
            break;
        }

        if (encoding & DW_EH_PE_indirect) {
            result = *(uintptr_t *)result;
        }
    }

    return (uintptr_t)result;
}



static bool isObjCExceptionCatcher(uintptr_t lsda, uintptr_t ip,
                                   const struct dwarf_eh_bases* bases,
                                   struct frame_range *frame)
{
    unsigned char LPStart_enc = *(const unsigned char *)lsda++;

    if (LPStart_enc != DW_EH_PE_omit) {
        read_address(&lsda, bases, LPStart_enc); // LPStart
    }

    unsigned char TType_enc = *(const unsigned char *)lsda++;
    if (TType_enc != DW_EH_PE_omit) {
        read_uleb(&lsda);  // TType
    }

    unsigned char call_site_enc = *(const unsigned char *)lsda++;
    uintptr_t length = read_uleb(&lsda);
    uintptr_t call_site_table = lsda;
    uintptr_t call_site_table_end = call_site_table + length;
    uintptr_t action_record_table = call_site_table_end;

    uintptr_t action_record = 0;
    uintptr_t p = call_site_table;

    uintptr_t try_start;
    uintptr_t try_end;
    uintptr_t try_landing_pad;

    while (p < call_site_table_end) {
        uintptr_t start   = read_address(&p, bases, call_site_enc)+bases->func;
        uintptr_t len     = read_address(&p, bases, call_site_enc);
        uintptr_t pad     = read_address(&p, bases, call_site_enc);
        uintptr_t action  = read_uleb(&p);

        if (ip < start) {
            // no more source ranges
            return false;
        }
        else if (ip < start + len) {
            // found the range
            if (!pad) return false;  // ...but it has no landing pad
            // found the landing pad
            action_record = action ? action_record_table + action - 1 : 0;
            try_start = start;
            try_end = start + len;
            try_landing_pad = pad;
            break;
        }
    }
    
    if (!action_record) return false;  // no catch handlers

    // has handlers, destructors, and/or throws specifications
    // Use this frame if it has any handlers
    bool has_handler = false;
    p = action_record;
    intptr_t offset;
    do {
        intptr_t filter = read_sleb(&p);
        uintptr_t temp = p;
        offset = read_sleb(&temp);
        p += offset;
        
        if (filter < 0) {
            // throws specification - ignore
        } else if (filter == 0) {
            // destructor - ignore
        } else /* filter >= 0 */ {
            // catch handler - use this frame
            has_handler = true;
            break;
        }
    } while (offset);

    if (!has_handler) return false;
    
    // Count the number of s  ource ranges with the same landing pad as our match
    unsigned int range_count = 0;
    p = call_site_table;
    while (p < call_site_table_end) {
                /*start*/  read_address(&p, bases, call_site_enc)/*+bases->func*/;
                /*len*/    read_address(&p, bases, call_site_enc);
        uintptr_t pad    = read_address(&p, bases, call_site_enc);
                /*action*/ read_uleb(&p);
        
        if (pad == try_landing_pad) {
            range_count++;
        }
    }

    if (range_count == 1) {
        // No other source ranges with the same landing pad. We're done here.
        frame->ips = nil;
    }
    else {
        // Record all ranges with the same landing pad as our match.
        frame->ips = (frame_ips *)
            malloc((range_count + 1) * sizeof(frame->ips[0]));
        unsigned int r = 0;
        p = call_site_table;
        while (p < call_site_table_end) {
            uintptr_t start  = read_address(&p, bases, call_site_enc)+bases->func;
            uintptr_t len    = read_address(&p, bases, call_site_enc);
            uintptr_t pad    = read_address(&p, bases, call_site_enc);
                    /*action*/ read_uleb(&p);
            
            if (pad == try_landing_pad) {
                if (start < try_start) try_start = start;
                if (start+len > try_end) try_end = start+len;
                frame->ips[r].start = start;
                frame->ips[r].end = start+len;
                r++;
            }
        }

        frame->ips[r].start = 0;
        frame->ips[r].end = 0;
    }

    frame->ip_start = try_start;
    frame->ip_end = try_end;

    return true;
}


static id ObjcExceptionPreprocessor(id exception) {
  //  static bool seen_first_exception = false;
  //
  //  // Record UMA and crash keys about the exception.
  //  RecordExceptionWithUma(exception);
  //
  //  static crash_reporter::CrashKeyString<256>
  //  firstexception("firstexception"); static
  //  crash_reporter::CrashKeyString<256> lastexception("lastexception");
  //
  //  static crash_reporter::CrashKeyString<1024> firstexception_bt(
  //      "firstexception_bt");
  //  static crash_reporter::CrashKeyString<1024> lastexception_bt(
  //      "lastexception_bt");
  //
  //  auto* key = seen_first_exception ? &lastexception : &firstexception;
  //  auto* bt_key = seen_first_exception ? &lastexception_bt :
  //  &firstexception_bt;
  //
  //  NSString* value = [NSString stringWithFormat:@"%@ reason %@",
  //      [exception name], [exception reason]];
  //  key->Set(base::SysNSStringToUTF8(value));
  //
  //  // This exception preprocessor runs prior to the one in libobjc, which
  //  sets
  //  // the -[NSException callStackReturnAddresses].
  //  crash_reporter::SetCrashKeyStringToStackTrace(bt_key,
  //                                                base::debug::StackTrace());
  //
  //  seen_first_exception = true;

  // Set a key here that we are going to serialize a dump for exception, and
  // but the signal handler will ignore it unless for-realz-crash-on-exception.

  //  ProcessSnapshotIOS process_snapshot;
  //  process_snapshot.Initialize(system_data);
  //  process_snapshot.SetNSException(exception);
//
//  {
//    unw_context_t context;
//    unw_getcontext(&context);
//
//    unw_cursor_t cursor;
//    unw_init_local(&cursor, &context);
//
//    // Get the base address for the image that contains this function.
//    Dl_info dl_info;
//    const void* this_base_address = 0;
//    if (dladdr(reinterpret_cast<const void*>(&ObjcExceptionPreprocessor),
//               &dl_info) != 0) {
//      this_base_address = dl_info.dli_fbase;
//    }
//
//    while (unw_step(&cursor) > 0) {
//      unw_proc_info_t info;
//      if (unw_get_proc_info(&cursor, &info) != UNW_ESUCCESS) {
//        continue;
//      }
//
//      if ( info.handler != (uintptr_t)__objc_personality_v0 )
//          continue;
//      // must have landing pad
//      if ( info.lsda == 0 )
//          continue;
//      // must have landing pad that catches objc exceptions
//      struct dwarf_eh_bases bases;
//      bases.tbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
//      bases.dbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
//      bases.func = info.start_ip;
//      unw_word_t reg_ip;
//      unw_get_reg(&cursor, UNW_REG_IP, &reg_ip);
//      reg_ip -= 1;
//      struct frame_range try_range = {0, 0, 0, 0};
//
//      char proc_name[64];
//      unw_word_t offset;
//      unw_get_proc_name(&cursor, proc_name, sizeof(proc_name), &offset);
//
//      unw_word_t ip, sp;
//      unw_get_reg(&cursor, UNW_REG_IP, &ip);
//      unw_get_reg(&cursor, UNW_REG_SP, &sp);
//
//
//      if ( isObjCExceptionCatcher(info.lsda, reg_ip, &bases, &try_range) ) {
//        LOG(INFO) << "proc_name is " << proc_name << " for ip " << std::hex
//                  << (uint64_t)ip << " for sp " << std::hex << (uint64_t)sp;
//      } else {
//      }
//    }
//  }
  //////////////////////////////////////////////////////////////////////////////

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

  while (unw_step(&cursor) > 0) {
    unw_proc_info_t frame_info;
    if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
      continue;
    }

    // This frame has an exception handler.
    if (frame_info.handler != 0) {
      
      // This frame is an objc handler, lets do some extra work.
      if ( frame_info.handler == (uintptr_t)__objc_personality_v0 ) {
        // must have landing pad
        if ( frame_info.lsda == 0 )
            continue;
        // must have landing pad that catches objc exceptions
        struct dwarf_eh_bases bases;
        bases.tbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
        bases.dbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
        bases.func = frame_info.start_ip;
        unw_word_t reg_ip;
        unw_get_reg(&cursor, UNW_REG_IP, &reg_ip);
        reg_ip -= 1;
        struct frame_range try_range = {0, 0, 0, 0};
        
        if (!isObjCExceptionCatcher(frame_info.lsda, reg_ip, &bases, &try_range) )
          continue;
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
