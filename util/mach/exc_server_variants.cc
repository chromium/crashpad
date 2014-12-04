// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/mach/exc_server_variants.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "util/mach/exc.h"
#include "util/mach/exception_behaviors.h"
#include "util/mach/excServer.h"
#include "util/mach/mach_exc.h"
#include "util/mach/mach_excServer.h"
#include "util/mach/mach_message.h"

extern "C" {

// These six functions are not used, and are in fact obsoleted by the other
// functionality implemented in this file. The standard MIG-generated exc_server
// (in excServer.c) and mach_exc_server (in mach_excServer.c) server dispatch
// routines usable with the standard mach_msg_server() function call out to
// these functions. exc_server() and mach_exc_server() are unused and are
// replaced by the more flexible ExcServer and MachExcServer, but the linker
// still needs to see these six function definitions.

kern_return_t catch_exception_raise(exception_handler_t exception_port,
                                    thread_t thread,
                                    task_t task,
                                    exception_type_t exception,
                                    exception_data_t code,
                                    mach_msg_type_number_t code_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_exception_raise_state(
    exception_handler_t exception_port,
    exception_type_t exception,
    exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_exception_raise_state_identity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise(exception_handler_t exception_port,
                                         thread_t thread,
                                         task_t task,
                                         exception_type_t exception,
                                         mach_exception_data_t code,
                                         mach_msg_type_number_t code_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise_state(
    exception_handler_t exception_port,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise_state_identity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

}  // extern "C"

namespace {

// There are no predefined constants for these.
enum MachMessageID : mach_msg_id_t {
  kMachMessageIDExceptionRaise = 2401,
  kMachMessageIDExceptionRaiseState = 2402,
  kMachMessageIDExceptionRaiseStateIdentity = 2403,
  kMachMessageIDMachExceptionRaise = 2405,
  kMachMessageIDMachExceptionRaiseState = 2406,
  kMachMessageIDMachExceptionRaiseStateIdentity = 2407,
};

// The MIG-generated __MIG_check__Request__*() functions are not declared as
// accepting const data, but they could have been because they in fact do not
// modify the data. These wrapper functions are provided to bridge the const gap
// between the code in this file, which is const-correct and treats request
// message data as const, and those generated functions.

kern_return_t MIGCheckRequestExceptionRaise(
    const __Request__exception_raise_t* in_request) {
  using Request = __Request__exception_raise_t;
  return __MIG_check__Request__exception_raise_t(
      const_cast<Request*>(in_request));
}

kern_return_t MIGCheckRequestExceptionRaiseState(
    const __Request__exception_raise_state_t* in_request,
    const __Request__exception_raise_state_t** in_request_1) {
  using Request = __Request__exception_raise_state_t;
  return __MIG_check__Request__exception_raise_state_t(
      const_cast<Request*>(in_request), const_cast<Request**>(in_request_1));
}

kern_return_t MIGCheckRequestExceptionRaiseStateIdentity(
    const __Request__exception_raise_state_identity_t* in_request,
    const __Request__exception_raise_state_identity_t** in_request_1) {
  using Request = __Request__exception_raise_state_identity_t;
  return __MIG_check__Request__exception_raise_state_identity_t(
      const_cast<Request*>(in_request), const_cast<Request**>(in_request_1));
}

kern_return_t MIGCheckRequestMachExceptionRaise(
    const __Request__mach_exception_raise_t* in_request) {
  using Request = __Request__mach_exception_raise_t;
  return __MIG_check__Request__mach_exception_raise_t(
      const_cast<Request*>(in_request));
}

kern_return_t MIGCheckRequestMachExceptionRaiseState(
    const __Request__mach_exception_raise_state_t* in_request,
    const __Request__mach_exception_raise_state_t** in_request_1) {
  using Request = __Request__mach_exception_raise_state_t;
  return __MIG_check__Request__mach_exception_raise_state_t(
      const_cast<Request*>(in_request), const_cast<Request**>(in_request_1));
}

kern_return_t MIGCheckRequestMachExceptionRaiseStateIdentity(
    const __Request__mach_exception_raise_state_identity_t* in_request,
    const __Request__mach_exception_raise_state_identity_t** in_request_1) {
  using Request = __Request__mach_exception_raise_state_identity_t;
  return __MIG_check__Request__mach_exception_raise_state_identity_t(
      const_cast<Request*>(in_request), const_cast<Request**>(in_request_1));
}

}  // namespace

namespace crashpad {
namespace internal {

ExcServer::ExcServer(ExcServer::Interface* interface)
    : MachMessageServer::Interface(),
      interface_(interface) {
}

bool ExcServer::MachMessageServerFunction(const mach_msg_header_t* in_header,
                                          mach_msg_header_t* out_header,
                                          bool* destroy_complex_request) {
  PrepareMIGReplyFromRequest(in_header, out_header);

  const mach_msg_trailer_t* in_trailer =
      MachMessageTrailerFromHeader(in_header);

  switch (in_header->msgh_id) {
    case kMachMessageIDExceptionRaise: {
      // exception_raise(), catch_exception_raise().
      using Request = __Request__exception_raise_t;
      const Request* in_request = reinterpret_cast<const Request*>(in_header);
      kern_return_t kr = MIGCheckRequestExceptionRaise(in_request);
      if (kr != MACH_MSG_SUCCESS) {
        SetMIGReplyError(out_header, kr);
        return true;
      }

      using Reply = __Reply__exception_raise_t;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->RetCode =
          interface_->CatchExceptionRaise(in_header->msgh_local_port,
                                          in_request->thread.name,
                                          in_request->task.name,
                                          in_request->exception,
                                          in_request->code,
                                          in_request->codeCnt,
                                          in_trailer,
                                          destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size = sizeof(*out_reply);
      return true;
    }

    case kMachMessageIDExceptionRaiseState: {
      // exception_raise_state(), catch_exception_raise_state().
      using Request = __Request__exception_raise_state_t;
      const Request* in_request = reinterpret_cast<const Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      const Request* in_request_1;
      kern_return_t kr =
          MIGCheckRequestExceptionRaiseState(in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetMIGReplyError(out_header, kr);
        return true;
      }

      using Reply = __Reply__exception_raise_state_t;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->flavor = in_request_1->flavor;
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode =
          interface_->CatchExceptionRaiseState(in_header->msgh_local_port,
                                               in_request->exception,
                                               in_request->code,
                                               in_request->codeCnt,
                                               &out_reply->flavor,
                                               in_request_1->old_state,
                                               in_request_1->old_stateCnt,
                                               out_reply->new_state,
                                               &out_reply->new_stateCnt,
                                               in_trailer);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }

    case kMachMessageIDExceptionRaiseStateIdentity: {
      // exception_raise_state_identity(),
      // catch_exception_raise_state_identity().
      using Request = __Request__exception_raise_state_identity_t;
      const Request* in_request = reinterpret_cast<const Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      const Request* in_request_1;
      kern_return_t kr =
          MIGCheckRequestExceptionRaiseStateIdentity(in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetMIGReplyError(out_header, kr);
        return true;
      }

      using Reply = __Reply__exception_raise_state_identity_t;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->flavor = in_request_1->flavor;
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode = interface_->CatchExceptionRaiseStateIdentity(
          in_header->msgh_local_port,
          in_request->thread.name,
          in_request->task.name,
          in_request->exception,
          in_request->code,
          in_request->codeCnt,
          &out_reply->flavor,
          in_request_1->old_state,
          in_request_1->old_stateCnt,
          out_reply->new_state,
          &out_reply->new_stateCnt,
          in_trailer,
          destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }
  }

  SetMIGReplyError(out_header, MIG_BAD_ID);
  return false;
}

std::set<mach_msg_id_t> ExcServer::MachMessageServerRequestIDs() {
  const mach_msg_id_t request_ids[] = {
      kMachMessageIDExceptionRaise,
      kMachMessageIDExceptionRaiseState,
      kMachMessageIDExceptionRaiseStateIdentity
  };
  return std::set<mach_msg_id_t>(
      &request_ids[0], &request_ids[arraysize(request_ids)]);
}

mach_msg_size_t ExcServer::MachMessageServerRequestSize() {
  return sizeof(__RequestUnion__exc_subsystem);
}

mach_msg_size_t ExcServer::MachMessageServerReplySize() {
  return sizeof(__ReplyUnion__exc_subsystem);
}

MachExcServer::MachExcServer(MachExcServer::Interface* interface)
    : MachMessageServer::Interface(),
      interface_(interface) {
}

bool MachExcServer::MachMessageServerFunction(
    const mach_msg_header_t* in_header,
    mach_msg_header_t* out_header,
    bool* destroy_complex_request) {
  PrepareMIGReplyFromRequest(in_header, out_header);

  const mach_msg_trailer_t* in_trailer =
      MachMessageTrailerFromHeader(in_header);

  switch (in_header->msgh_id) {
    case kMachMessageIDMachExceptionRaise: {
      // mach_exception_raise(), catch_mach_exception_raise().
      using Request = __Request__mach_exception_raise_t;
      const Request* in_request = reinterpret_cast<const Request*>(in_header);
      kern_return_t kr = MIGCheckRequestMachExceptionRaise(in_request);
      if (kr != MACH_MSG_SUCCESS) {
        SetMIGReplyError(out_header, kr);
        return true;
      }

      using Reply = __Reply__mach_exception_raise_t;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->RetCode =
          interface_->CatchMachExceptionRaise(in_header->msgh_local_port,
                                              in_request->thread.name,
                                              in_request->task.name,
                                              in_request->exception,
                                              in_request->code,
                                              in_request->codeCnt,
                                              in_trailer,
                                              destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size = sizeof(*out_reply);
      return true;
    }

    case kMachMessageIDMachExceptionRaiseState: {
      // mach_exception_raise_state(), catch_mach_exception_raise_state().
      using Request = __Request__mach_exception_raise_state_t;
      const Request* in_request = reinterpret_cast<const Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      const Request* in_request_1;
      kern_return_t kr =
          MIGCheckRequestMachExceptionRaiseState(in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetMIGReplyError(out_header, kr);
        return true;
      }

      using Reply = __Reply__mach_exception_raise_state_t;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->flavor = in_request_1->flavor;
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode =
          interface_->CatchMachExceptionRaiseState(in_header->msgh_local_port,
                                                   in_request->exception,
                                                   in_request->code,
                                                   in_request->codeCnt,
                                                   &out_reply->flavor,
                                                   in_request_1->old_state,
                                                   in_request_1->old_stateCnt,
                                                   out_reply->new_state,
                                                   &out_reply->new_stateCnt,
                                                   in_trailer);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }

    case kMachMessageIDMachExceptionRaiseStateIdentity: {
      // mach_exception_raise_state_identity(),
      // catch_mach_exception_raise_state_identity().
      using Request = __Request__mach_exception_raise_state_identity_t;
      const Request* in_request = reinterpret_cast<const Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      const Request* in_request_1;
      kern_return_t kr = MIGCheckRequestMachExceptionRaiseStateIdentity(
          in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetMIGReplyError(out_header, kr);
        return true;
      }

      using Reply = __Reply__mach_exception_raise_state_identity_t;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->flavor = in_request_1->flavor;
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode = interface_->CatchMachExceptionRaiseStateIdentity(
          in_header->msgh_local_port,
          in_request->thread.name,
          in_request->task.name,
          in_request->exception,
          in_request->code,
          in_request->codeCnt,
          &out_reply->flavor,
          in_request_1->old_state,
          in_request_1->old_stateCnt,
          out_reply->new_state,
          &out_reply->new_stateCnt,
          in_trailer,
          destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }
  }

  SetMIGReplyError(out_header, MIG_BAD_ID);
  return false;
}

std::set<mach_msg_id_t> MachExcServer::MachMessageServerRequestIDs() {
  const mach_msg_id_t request_ids[] = {
      kMachMessageIDMachExceptionRaise,
      kMachMessageIDMachExceptionRaiseState,
      kMachMessageIDMachExceptionRaiseStateIdentity
  };
  return std::set<mach_msg_id_t>(
      &request_ids[0], &request_ids[arraysize(request_ids)]);
}

mach_msg_size_t MachExcServer::MachMessageServerRequestSize() {
  return sizeof(__RequestUnion__mach_exc_subsystem);
}

mach_msg_size_t MachExcServer::MachMessageServerReplySize() {
  return sizeof(__ReplyUnion__mach_exc_subsystem);
}

SimplifiedExcServer::SimplifiedExcServer(
    SimplifiedExcServer::Interface* interface)
    : ExcServer(this),
      ExcServer::Interface(),
      interface_(interface) {
}

kern_return_t SimplifiedExcServer::CatchExceptionRaise(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    const mach_msg_trailer_t* trailer,
    bool* destroy_request) {
  thread_state_flavor_t flavor = THREAD_STATE_NONE;
  mach_msg_type_number_t new_state_count = 0;
  return interface_->CatchException(EXCEPTION_DEFAULT,
                                    exception_port,
                                    thread,
                                    task,
                                    exception,
                                    code_count ? code : nullptr,
                                    code_count,
                                    &flavor,
                                    nullptr,
                                    0,
                                    nullptr,
                                    &new_state_count,
                                    trailer,
                                    destroy_request);
}

kern_return_t SimplifiedExcServer::CatchExceptionRaiseState(
    exception_handler_t exception_port,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    const mach_msg_trailer_t* trailer) {
  bool destroy_complex_request = false;
  return interface_->CatchException(EXCEPTION_STATE,
                                    exception_port,
                                    THREAD_NULL,
                                    TASK_NULL,
                                    exception,
                                    code_count ? code : nullptr,
                                    code_count,
                                    flavor,
                                    old_state_count ? old_state : nullptr,
                                    old_state_count,
                                    new_state_count ? new_state : nullptr,
                                    new_state_count,
                                    trailer,
                                    &destroy_complex_request);
}

kern_return_t SimplifiedExcServer::CatchExceptionRaiseStateIdentity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    const mach_msg_trailer_t* trailer,
    bool* destroy_request) {
  return interface_->CatchException(EXCEPTION_STATE_IDENTITY,
                                    exception_port,
                                    thread,
                                    task,
                                    exception,
                                    code_count ? code : nullptr,
                                    code_count,
                                    flavor,
                                    old_state_count ? old_state : nullptr,
                                    old_state_count,
                                    new_state_count ? new_state : nullptr,
                                    new_state_count,
                                    trailer,
                                    destroy_request);
}

SimplifiedMachExcServer::SimplifiedMachExcServer(
    SimplifiedMachExcServer::Interface* interface)
    : MachExcServer(this),
      MachExcServer::Interface(),
      interface_(interface) {
}

kern_return_t SimplifiedMachExcServer::CatchMachExceptionRaise(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    const mach_msg_trailer_t* trailer,
    bool* destroy_request) {
  thread_state_flavor_t flavor = THREAD_STATE_NONE;
  mach_msg_type_number_t new_state_count = 0;
  return interface_->CatchMachException(
      EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
      exception_port,
      thread,
      task,
      exception,
      code_count ? code : nullptr,
      code_count,
      &flavor,
      nullptr,
      0,
      nullptr,
      &new_state_count,
      trailer,
      destroy_request);
}

kern_return_t SimplifiedMachExcServer::CatchMachExceptionRaiseState(
    exception_handler_t exception_port,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    const mach_msg_trailer_t* trailer) {
  bool destroy_complex_request = false;
  return interface_->CatchMachException(EXCEPTION_STATE | MACH_EXCEPTION_CODES,
                                        exception_port,
                                        THREAD_NULL,
                                        TASK_NULL,
                                        exception,
                                        code_count ? code : nullptr,
                                        code_count,
                                        flavor,
                                        old_state_count ? old_state : nullptr,
                                        old_state_count,
                                        new_state_count ? new_state : nullptr,
                                        new_state_count,
                                        trailer,
                                        &destroy_complex_request);
}

kern_return_t SimplifiedMachExcServer::CatchMachExceptionRaiseStateIdentity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    const mach_msg_trailer_t* trailer,
    bool* destroy_request) {
  return interface_->CatchMachException(
      EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
      exception_port,
      thread,
      task,
      exception,
      code_count ? code : nullptr,
      code_count,
      flavor,
      old_state_count ? old_state : nullptr,
      old_state_count,
      new_state_count ? new_state : nullptr,
      new_state_count,
      trailer,
      destroy_request);
}

}  // namespace internal

UniversalMachExcServer::UniversalMachExcServer(
    UniversalMachExcServer::Interface* interface)
    : MachMessageServer::Interface(),
      internal::SimplifiedExcServer::Interface(),
      internal::SimplifiedMachExcServer::Interface(),
      exc_server_(this),
      mach_exc_server_(this),
      interface_(interface) {
}

bool UniversalMachExcServer::MachMessageServerFunction(
    const mach_msg_header_t* in_header,
    mach_msg_header_t* out_header,
    bool* destroy_complex_request) {
  switch (in_header->msgh_id) {
    case kMachMessageIDMachExceptionRaise:
    case kMachMessageIDMachExceptionRaiseState:
    case kMachMessageIDMachExceptionRaiseStateIdentity:
      return mach_exc_server_.MachMessageServerFunction(
          in_header, out_header, destroy_complex_request);
    case kMachMessageIDExceptionRaise:
    case kMachMessageIDExceptionRaiseState:
    case kMachMessageIDExceptionRaiseStateIdentity:
      return exc_server_.MachMessageServerFunction(
          in_header, out_header, destroy_complex_request);
  }

  // Do what the MIG-generated server routines do when they can’t dispatch a
  // message.
  PrepareMIGReplyFromRequest(in_header, out_header);
  SetMIGReplyError(out_header, MIG_BAD_ID);
  return false;
}

std::set<mach_msg_id_t> UniversalMachExcServer::MachMessageServerRequestIDs() {
  std::set<mach_msg_id_t> request_ids =
      exc_server_.MachMessageServerRequestIDs();

  std::set<mach_msg_id_t> mach_exc_request_ids =
      mach_exc_server_.MachMessageServerRequestIDs();
  request_ids.insert(mach_exc_request_ids.begin(), mach_exc_request_ids.end());

  return request_ids;
}

mach_msg_size_t UniversalMachExcServer::MachMessageServerRequestSize() {
  return std::max(mach_exc_server_.MachMessageServerRequestSize(),
                  exc_server_.MachMessageServerRequestSize());
}

mach_msg_size_t UniversalMachExcServer::MachMessageServerReplySize() {
  return std::max(mach_exc_server_.MachMessageServerReplySize(),
                  exc_server_.MachMessageServerReplySize());
}

kern_return_t UniversalMachExcServer::CatchException(
    exception_behavior_t behavior,
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    const mach_msg_trailer_t* trailer,
    bool* destroy_complex_request) {
  std::vector<mach_exception_data_type_t> mach_codes;
  mach_codes.reserve(code_count);
  for (size_t index = 0; index < code_count; ++index) {
    mach_codes.push_back(code[index]);
  }

  return interface_->CatchMachException(behavior,
                                        exception_port,
                                        thread,
                                        task,
                                        exception,
                                        code_count ? &mach_codes[0] : nullptr,
                                        code_count,
                                        flavor,
                                        old_state_count ? old_state : nullptr,
                                        old_state_count,
                                        new_state_count ? new_state : nullptr,
                                        new_state_count,
                                        trailer,
                                        destroy_complex_request);
}

kern_return_t UniversalMachExcServer::CatchMachException(
    exception_behavior_t behavior,
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    const mach_msg_trailer_t* trailer,
    bool* destroy_complex_request) {
  return interface_->CatchMachException(behavior,
                                        exception_port,
                                        thread,
                                        task,
                                        exception,
                                        code_count ? code : nullptr,
                                        code_count,
                                        flavor,
                                        old_state_count ? old_state : nullptr,
                                        old_state_count,
                                        new_state_count ? new_state : nullptr,
                                        new_state_count,
                                        trailer,
                                        destroy_complex_request);
}

exception_type_t ExcCrashRecoverOriginalException(
    mach_exception_code_t code_0,
    mach_exception_code_t* original_code_0,
    int* signal) {
  // 10.9.4 xnu-2422.110.17/bsd/kern/kern_exit.c proc_prepareexit() sets code[0]
  // based on the signal value, original exception type, and low 20 bits of the
  // original code[0] before calling xnu-2422.110.17/osfmk/kern/exception.c
  // task_exception_notify() to raise an EXC_CRASH.
  //
  // The list of core-generating signals (as used in proc_prepareexit()’s call
  // to hassigprop()) is in 10.9.4 xnu-2422.110.17/bsd/sys/signalvar.h sigprop:
  // entires with SA_CORE are in the set. These signals are SIGQUIT, SIGILL,
  // SIGTRAP, SIGABRT, SIGEMT, SIGFPE, SIGBUS, SIGSEGV, and SIGSYS. Processes
  // killed for code-signing reasons will be killed by SIGKILL and are also
  // eligible for EXC_CRASH handling, but processes killed by SIGKILL for other
  // reasons are not.
  if (signal) {
    *signal = (code_0 >> 24) & 0xff;
  }

  if (original_code_0) {
    *original_code_0 = code_0 & 0xfffff;
  }

  return (code_0 >> 20) & 0xf;
}

kern_return_t ExcServerSuccessfulReturnValue(exception_behavior_t behavior,
                                             bool set_thread_state) {
  if (!set_thread_state && ExceptionBehaviorHasState(behavior)) {
    return MACH_RCV_PORT_DIED;
  }

  return KERN_SUCCESS;
}

}  // namespace crashpad
