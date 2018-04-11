// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "util/net/http_transport.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "base/logging.h"

namespace crashpad {

namespace {

class HTTPTransportBoringSSL final : public HTTPTransport {
 public:
  HTTPTransportBoringSSL() = default;
  ~HTTPTransportBoringSSL() override = default;

  bool ExecuteSynchronously(std::string* response_body) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPTransportBoringSSL);
};

bool HTTPTransportBoringSSL::ExecuteSynchronously(std::string* response_body) {
  CRYPTO_library_init();
  return false;
}

}  // namespace

// static
std::unique_ptr<HTTPTransport> HTTPTransport::Create() {
  return std::unique_ptr<HTTPTransportBoringSSL>(new HTTPTransportBoringSSL);
  }

}  // namespace crashpad
