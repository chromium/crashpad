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

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/stringprintf.h"
#include "util/file/file_io.h"
#include "util/net/http_body.h"

namespace crashpad {

namespace {

struct ScopedSSLCTXTraits {
  static SSL_CTX* InvalidValue() { return nullptr; }
  static void Free(SSL_CTX* ctx) { SSL_CTX_free(ctx); }
};
using ScopedSSLCTX = base::ScopedGeneric<SSL_CTX*, ScopedSSLCTXTraits>;

struct ScopedBIOTraits {
  static BIO* InvalidValue() { return nullptr; }
  static void Free(BIO* bio) { BIO_free_all(bio); }
};
using ScopedBIO = base::ScopedGeneric<BIO*, ScopedBIOTraits>;

class HTTPTransportBoringSSL final : public HTTPTransport {
 public:
  HTTPTransportBoringSSL() = default;
  ~HTTPTransportBoringSSL() override = default;

  bool ExecuteSynchronously(std::string* response_body) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPTransportBoringSSL);
};

#define SSL_ERROR(func) \
  LOG(ERROR) << func << ": " << ERR_error_string(ERR_get_error(), nullptr);

ScopedSSLCTX CreateSSLContext() {
  ScopedSSLCTX ctx(SSL_CTX_new(TLS_method()));
  if (!ctx.is_valid()) {
    SSL_ERROR("SSL_CTX_new");
    return ScopedSSLCTX();
  }

  if (SSL_CTX_set_min_proto_version(ctx.get(), TLS1_VERSION) <= 0) {
    SSL_ERROR("SSL_CTX_set_min_proto_version");
    return ScopedSSLCTX();
  }

  SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
  SSL_CTX_set_verify_depth(ctx.get(), 5);

  // TODO(scottmg): Configurable location for Fuchsia.
  if (SSL_CTX_load_verify_locations(ctx.get(), nullptr, "/etc/ssl/certs") <=
      0) {
    SSL_ERROR("SSL_CTX_load_verify_locations");
    return ScopedSSLCTX();
  }

  return ctx;
}

ScopedBIO OpenSSLConnection(SSL_CTX* ctx,
                            const char* hostname,
                            const char* hostname_and_port) {
  ScopedBIO bio(BIO_new_ssl_connect(ctx));
  if (!bio.is_valid()) {
    SSL_ERROR("BIO_new_ssl_connect");
    return ScopedBIO();
  }

  if (BIO_set_conn_hostname(bio.get(), hostname_and_port) <= 0) {
    SSL_ERROR("BIO_set_conn_hostname");
    return ScopedBIO();
  }

  SSL* ssl;
  if (BIO_get_ssl(bio.get(), &ssl) <= 0) {
    SSL_ERROR("BIO_get_ssl");
    return ScopedBIO();
  }

  // This enables SNI.
  if (SSL_set_tlsext_host_name(ssl, hostname) <= 0) {
    SSL_ERROR("SSL_set_tlsext_host_name");
    return ScopedBIO();
  }

  if (BIO_do_connect(bio.get()) <= 0) {
    SSL_ERROR("BIO_do_connect");
    return ScopedBIO();
  }

  return bio;
}

bool HTTPTransportBoringSSL::ExecuteSynchronously(std::string* response_body) {
  // TODO(scottmg): Maybe need to manually init library if not BoringSSL. Or in
  // Chrome. Or both or something?

  std::string hostname = "httpbin.org";
  std::string port = "443";
  std::string destination = hostname + ":" + port;

  ScopedSSLCTX ctx(CreateSSLContext());
  if (!ctx.is_valid()) {
    return false;
  }

  ScopedBIO bio(
      OpenSSLConnection(ctx.get(), hostname.c_str(), destination.c_str()));
  if (!bio.is_valid()) {
    return false;
  }

  std::string prelude(
      "POST /post HTTP/1.1\r\n"
      "Host: " + hostname + "\r\n"
      "Connection: close\r\n"
      "Transfer-Encoding: chunked\r\n");

  for (const auto& kv : headers()) {
    prelude += kv.first + ": " + kv.second + "\r\n";
  }
  prelude += "\r\n";

  for (;;) {
    // Read next block of data from client.
    uint8_t buf[4096];
    FileOperationResult size = body_stream()->GetBytesBuffer(buf, sizeof(buf));

    // Write chunk size to stream.
    std::string chunked(base::StringPrintf("%zx\r\n", size));
    BIO_write(bio.get(), chunked.c_str(), chunked.size());
    if (size == 0)  // Note, break after writing 0-length terminator chunk.
      break;

    // Write chunk data, followed by chunk terminator.
    BIO_write(bio.get(), buf, size);
    BIO_write(bio.get(), "\r\n", 2);  // Chunk terminator.
  }

  // Terminator for chunked transfer.
  BIO_write(bio.get(), "\r\n", 2);  // Chunked body terminator.

  if (response_body) {
    response_body->clear();

    int bytes_read = 0;
    do {
      char read_buffer[4096];

      bytes_read = BIO_read(bio.get(), read_buffer, sizeof(read_buffer));
      if (bytes_read > 0) {
        response_body->append(read_buffer, bytes_read);
      }
    } while (bytes_read > 0);
  }

  return true;
}

}  // namespace

// static
std::unique_ptr<HTTPTransport> HTTPTransport::Create() {
  return std::unique_ptr<HTTPTransportBoringSSL>(new HTTPTransportBoringSSL);
}

}  // namespace crashpad
