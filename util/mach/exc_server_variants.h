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

#ifndef CRASHPAD_UTIL_MACH_EXC_SERVER_VARIANTS_H_
#define CRASHPAD_UTIL_MACH_EXC_SERVER_VARIANTS_H_

#include <mach/mach.h>

#include "util/mach/mach_message_server.h"

namespace crashpad {

// Routines to provide a single unified front-end to the interfaces in
// <mach/exc.defs> and <mach/mach_exc.defs>. The two interfaces are identical,
// except that the latter allows for 64-bit exception codes, and is requested by
// setting the MACH_EXCEPTION_CODES behavior bit associated with an exception
// port.

namespace internal {

//! \brief A server interface for the `exc` Mach subsystem.
class ExcServer : public MachMessageServer::Interface {
 public:
  //! \brief An interface that the different request messages that are a part of
  //!     the `exc` Mach subsystem can be dispatched to.
  class Interface {
   public:
    //! \brief Handles exceptions raised by `exception_raise()`.
    //!
    //! This behaves equivalently to a `catch_exception_raise()` function used
    //! with `exc_server()`.
    //!
    //! \param[out] destroy_request `true` if the request message is to be
    //!     destroyed even when this method returns success. See
    //!     MachMessageServer::Interface.
    virtual kern_return_t CatchExceptionRaise(
        exception_handler_t exception_port,
        thread_t thread,
        task_t task,
        exception_type_t exception,
        const exception_data_type_t* code,
        mach_msg_type_number_t code_count,
        bool* destroy_request) = 0;

    //! \brief Handles exceptions raised by `exception_raise_state()`.
    //!
    //! This behaves equivalently to a `catch_exception_raise_state()` function
    //! used with `exc_server()`.
    //!
    //! There is no \a destroy_request parameter because, unlike
    //! CatchExceptionRaise() and CatchExceptionRaiseStateIdentity(), the
    //! request message is not complex (it does not carry the \a thread or \a
    //! task port rights) and thus there is nothing to destroy.
    virtual kern_return_t CatchExceptionRaiseState(
        exception_handler_t exception_port,
        exception_type_t exception,
        const exception_data_type_t* code,
        mach_msg_type_number_t code_count,
        thread_state_flavor_t* flavor,
        const natural_t* old_state,
        mach_msg_type_number_t old_state_count,
        thread_state_t new_state,
        mach_msg_type_number_t* new_state_count) = 0;

    //! \brief Handles exceptions raised by `exception_raise_state_identity()`.
    //!
    //! This behaves equivalently to a `catch_exception_raise_state_identity()`
    //! function used with `exc_server()`.
    //!
    //! \param[out] destroy_request `true` if the request message is to be
    //!     destroyed even when this method returns success. See
    //!     MachMessageServer::Interface.
    virtual kern_return_t CatchExceptionRaiseStateIdentity(
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
        bool* destroy_request) = 0;

   protected:
    ~Interface() {}
  };

  explicit ExcServer(Interface* interface);

  // MachMessageServer::Interface:

  virtual bool MachMessageServerFunction(
      const mach_msg_header_t* in_header,
      mach_msg_header_t* out_header,
      bool* destroy_complex_request) override;

  virtual mach_msg_size_t MachMessageServerRequestSize() override;
  virtual mach_msg_size_t MachMessageServerReplySize() override;

 private:
  Interface* interface_;  // weak
};

//! \brief A server interface for the `mach_exc` Mach subsystem.
class MachExcServer : public MachMessageServer::Interface {
 public:
  //! \brief An interface that the different request messages that are a part of
  //!     the `mach_exc` Mach subsystem can be dispatched to.
  class Interface {
   public:
    //! \brief Handles exceptions raised by `mach_exception_raise()`.
    //!
    //! This behaves equivalently to a `catch_mach_exception_raise()` function
    //! used with `mach_exc_server()`.
    //!
    //! \param[out] destroy_request `true` if the request message is to be
    //!     destroyed even when this method returns success. See
    //!     MachMessageServer::Interface.
    virtual kern_return_t CatchMachExceptionRaise(
        exception_handler_t exception_port,
        thread_t thread,
        task_t task,
        exception_type_t exception,
        const mach_exception_data_type_t* code,
        mach_msg_type_number_t code_count,
        bool* destroy_request) = 0;

    //! \brief Handles exceptions raised by `mach_exception_raise_state()`.
    //!
    //! This behaves equivalently to a `catch_mach_exception_raise_state()`
    //! function used with `mach_exc_server()`.
    //!
    //! There is no \a destroy_request parameter because, unlike
    //! CatchMachExceptionRaise() and CatchMachExceptionRaiseStateIdentity(),
    //! the request message is not complex (it does not carry the \a thread or
    //! \a task port rights) and thus there is nothing to destroy.
    virtual kern_return_t CatchMachExceptionRaiseState(
        exception_handler_t exception_port,
        exception_type_t exception,
        const mach_exception_data_type_t* code,
        mach_msg_type_number_t code_count,
        thread_state_flavor_t* flavor,
        const natural_t* old_state,
        mach_msg_type_number_t old_state_count,
        thread_state_t new_state,
        mach_msg_type_number_t* new_state_count) = 0;

    //! \brief Handles exceptions raised by
    //!     `mach_exception_raise_state_identity()`.
    //!
    //! This behaves equivalently to a
    //! `catch_mach_exception_raise_state_identity()` function used with
    //! `mach_exc_server()`.
    //!
    //! \param[out] destroy_request `true` if the request message is to be
    //!     destroyed even when this method returns success. See
    //!     MachMessageServer::Interface.
    virtual kern_return_t CatchMachExceptionRaiseStateIdentity(
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
        bool* destroy_request) = 0;

   protected:
    ~Interface() {}
  };

  explicit MachExcServer(Interface* interface);

  // MachMessageServer::Interface:

  virtual bool MachMessageServerFunction(
      const mach_msg_header_t* in_header,
      mach_msg_header_t* out_header,
      bool* destroy_complex_request) override;

  virtual mach_msg_size_t MachMessageServerRequestSize() override;
  virtual mach_msg_size_t MachMessageServerReplySize() override;

 private:
  Interface* interface_;  // weak
};

//! \brief A server interface for the `exc` Mach subsystem, simplified to have
//!     only a single interface method needing implementation.
class SimplifiedExcServer : public ExcServer, public ExcServer::Interface {
 public:
  //! \brief An interface that the different request messages that are a part of
  //!     the `exc` Mach subsystem can be dispatched to.
  class Interface {
   public:
    //! \brief Handles exceptions raised by `exception_raise()`,
    //!     `exception_raise_state()`, and `exception_raise_state_identity()`.
    //!
    //! For convenience in implementation, these different “behaviors” of
    //! exception messages are all mapped to a single interface method. The
    //! exception’s original “behavior” is specified in the \a behavior
    //! parameter. Only parameters that were supplied in the request message
    //! are populated, other parameters are set to reasonable default values.
    //!
    //! The meanings of most parameters are identical to that of
    //! ExcServer::Interface::CatchExceptionRaiseStateIdentity().
    //!
    //! \param[in] behavior `EXCEPTION_DEFAULT`, `EXCEPTION_STATE`, or
    //!     `EXCEPTION_STATE_IDENTITY`, identifying which exception request
    //!     message was processed and thus which other parameters are valid.
    virtual kern_return_t CatchException(
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
        bool* destroy_complex_request) = 0;

   protected:
    ~Interface() {}
  };

  explicit SimplifiedExcServer(Interface* interface);

  // ExcServer::Interface:

  virtual kern_return_t CatchExceptionRaise(
      exception_handler_t exception_port,
      thread_t thread,
      task_t task,
      exception_type_t exception,
      const exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      bool* destroy_request) override;
  virtual kern_return_t CatchExceptionRaiseState(
      exception_handler_t exception_port,
      exception_type_t exception,
      const exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      thread_state_flavor_t* flavor,
      const natural_t* old_state,
      mach_msg_type_number_t old_state_count,
      thread_state_t new_state,
      mach_msg_type_number_t* new_state_count) override;
  virtual kern_return_t CatchExceptionRaiseStateIdentity(
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
      bool* destroy_request) override;

 private:
  Interface* interface_;  // weak
};

//! \brief A server interface for the `mach_exc` Mach subsystem, simplified to
//!     have only a single interface method needing implementation.
class SimplifiedMachExcServer : public MachExcServer,
                                public MachExcServer::Interface {
 public:
  //! \brief An interface that the different request messages that are a part of
  //!     the `mach_exc` Mach subsystem can be dispatched to.
  class Interface {
   public:
    //! \brief Handles exceptions raised by `mach_exception_raise()`,
    //!     `mach_exception_raise_state()`, and
    //!     `mach_exception_raise_state_identity()`.
    //!
    //! When used with UniversalMachExcServer, this also handles exceptions
    //! raised by `exception_raise()`, `exception_raise_state()`, and
    //! `exception_raise_state_identity()`.
    //!
    //! For convenience in implementation, these different “behaviors” of
    //! exception messages are all mapped to a single interface method. The
    //! exception’s original “behavior” is specified in the \a behavior
    //! parameter. Only parameters that were supplied in the request message
    //! are populated, other parameters are set to reasonable default values.
    //!
    //! The meanings of most parameters are identical to that of
    //! MachExcServer::Interface::CatchMachExceptionRaiseStateIdentity().
    //!
    //! \param[in] behavior `MACH_EXCEPTION_CODES | EXCEPTION_DEFAULT`,
    //!     `MACH_EXCEPTION_CODES | EXCEPTION_STATE`, or
    //!     `MACH_EXCEPTION_CODES | EXCEPTION_STATE_IDENTITY`, identifying which
    //!     exception request message was processed and thus which other
    //!     parameters are valid. When used with UniversalMachExcServer, \a
    //!     behavior can also be `EXCEPTION_DEFAULT`, `EXCEPTION_STATE`, or
    //!     `EXCEPTION_STATE_IDENTITY`.
    virtual kern_return_t CatchMachException(
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
        bool* destroy_complex_request) = 0;

   protected:
    ~Interface() {}
  };

  explicit SimplifiedMachExcServer(Interface* interface);

  // MachExcServer::Interface:

  virtual kern_return_t CatchMachExceptionRaise(
      exception_handler_t exception_port,
      thread_t thread,
      task_t task,
      exception_type_t exception,
      const mach_exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      bool* destroy_request) override;
  virtual kern_return_t CatchMachExceptionRaiseState(
      exception_handler_t exception_port,
      exception_type_t exception,
      const mach_exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      thread_state_flavor_t* flavor,
      const natural_t* old_state,
      mach_msg_type_number_t old_state_count,
      thread_state_t new_state,
      mach_msg_type_number_t* new_state_count) override;
  virtual kern_return_t CatchMachExceptionRaiseStateIdentity(
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
      bool* destroy_request) override;

 private:
  Interface* interface_;  // weak
};

}  // namespace internal

//! \brief A server interface for the `exc` and `mach_exc` Mach subsystems,
//!     unified to handle exceptions delivered to either subsystem, and
//!     simplified to have only a single interface method needing
//!     implementation.
//!
//! UniversalMachExcServer operates by translating messages received in the
//! `exc` subsystem to a variant that is compatible with the `mach_exc`
//! subsystem. This involves changing the format of \a code, the exception code
//! field, from `exception_data_type_t` to `mach_exception_data_type_t`.
//! This is achieved by implementing SimplifiedExcServer::Interface and having
//! it forward translated messages to SimplifiedMachExcServer::Interface, which
//! is left unimplemented here so that users of this class can provide their own
//! implementations.
class UniversalMachExcServer
    : public MachMessageServer::Interface,
      public internal::SimplifiedExcServer::Interface,
      public internal::SimplifiedMachExcServer::Interface {
 public:
  UniversalMachExcServer();

  // MachMessageServer::Interface:

  virtual bool MachMessageServerFunction(
      const mach_msg_header_t* in_header,
      mach_msg_header_t* out_header,
      bool* destroy_complex_request) override;

  virtual mach_msg_size_t MachMessageServerRequestSize() override;
  virtual mach_msg_size_t MachMessageServerReplySize() override;

  // internal::SimplifiedExcServer::Interface:

  virtual kern_return_t CatchException(
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
      bool* destroy_complex_request) override;

 private:
  internal::SimplifiedExcServer exc_server_;
  internal::SimplifiedMachExcServer mach_exc_server_;
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MACH_EXC_SERVER_VARIANTS_H_
