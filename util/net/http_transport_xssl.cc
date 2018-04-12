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

void LogError(const char* func) {
  LOG(ERROR) << func << ": " << ERR_error_string(ERR_get_error(), nullptr);
}

ScopedSSLCTX CreateSSLContext() {
  ScopedSSLCTX ctx(SSL_CTX_new(TLS_method()));
  if (!ctx.is_valid()) {
    LogError("SSL_CTX_new");
    return ScopedSSLCTX();
  }

  if (SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION) <= 0) {
    LogError("SSL_CTX_set_min_proto_version");
    return ScopedSSLCTX();
  }

  SSL_CTX_set_verify(
      ctx.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
  SSL_CTX_set_verify_depth(ctx.get(), 5);

  // TODO(scottmg): Configurable location for Fuchsia.
  if (SSL_CTX_load_verify_locations(ctx.get(), nullptr, "/etc/ssl/certs") <=
      0) {
    LogError("SSL_CTX_load_verify_locations");
    return ScopedSSLCTX();
  }

  return ctx;
}

ScopedBIO OpenSSLConnection(SSL_CTX* ctx, const char* server) {
  ScopedBIO bio(BIO_new_ssl_connect(ctx));
  if (!bio.is_valid()) {
    LogError("BIO_new_connect");
    return ScopedBIO();
  }

  if (BIO_set_conn_hostname(bio.get(), server) <= 0) {
    LogError("BIO_set_conn_hostname");
    return ScopedBIO();
  }

  if (BIO_do_connect(bio.get()) <= 0) {
    LogError("BIO_do_connect");
    return ScopedBIO();
  }

  if (BIO_do_handshake(bio.get()) <= 0) {
    LogError("BIO_do_handshake");
    return ScopedBIO();
  }

  return bio;
}

bool CheckCertificate(BIO* bio, const char* hostname) {
  SSL* ssl;
  BIO_get_ssl(bio, &ssl);
  if (!ssl) {
    LogError("BIO_get_ssl");
    return false;
  }

  X509* cert = SSL_get_peer_certificate(ssl);
  if (!cert) {
    LogError("SSL_get_peer_certificate");
    return false;
  }

  if (SSL_get_verify_result(ssl) != X509_V_OK) {
    LogError("SSL_get_verify_result");
    return false;
  }

  X509_NAME* subject_name = X509_get_subject_name(cert);
  if (!subject_name) {
    LogError("X509_get_subject_name");
    return false;
  }

  X509_NAME_print_ex_fp(stderr, subject_name, 0, 0);

  // TODO: If I connect to google.ca instead, the common name is google.com
  // still, so this is presumably not right.

  // Loop through hostnames in cert (CN is "common names").
  for (int pos = -1;;) {
    pos = X509_NAME_get_index_by_NID(subject_name, NID_commonName, pos);

    if (pos == -1) {
      break;
    }

    X509_NAME_ENTRY* cn = X509_NAME_get_entry(subject_name, pos);
    ASN1_STRING* asn1 = X509_NAME_ENTRY_get_data(cn);
    unsigned char* cn_str;
    if (ASN1_STRING_to_UTF8(&cn_str, asn1) < 0) {
      LogError("ASN1_STRING_to_UTF8");
      return false;
    }

    if (strcmp(reinterpret_cast<const char*>(cn_str), hostname) == 0) {
      return true;
    }
  }

  return false;
}

bool HTTPTransportBoringSSL::ExecuteSynchronously(std::string* response_body) {
  // TODO: Maybe need to init library if not Boring. Or in Chrome. Or something?

  std::string hostname = "www.google.com";
  std::string port = "443";
  std::string destination = hostname + ":" + port;

  ScopedSSLCTX ctx(CreateSSLContext());
  if (!ctx.is_valid()) {
    return false;
  }

  ScopedBIO bio(OpenSSLConnection(ctx.get(), destination.c_str()));
  if (!bio.is_valid()) {
    return false;
  }

  if (!CheckCertificate(bio.get(), hostname.c_str())) {
    LOG(ERROR) << "CheckCertificate";
    return false;
  }

  static constexpr char kTestRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: www.google.com\r\n"
      "Connection: close\r\n\r\n";
  BIO_puts(bio.get(), kTestRequest);

  /* receive response */
  int size;
  do {
    char buf[1024];
    size = BIO_read(bio.get(), buf, sizeof(buf));
    if (size > 0) {
      fwrite(buf, 1, size, stdout);
    }
  } while (size > 0 || BIO_should_retry(bio.get()));

  return true;
}

}  // namespace

// static
std::unique_ptr<HTTPTransport> HTTPTransport::Create() {
  return std::unique_ptr<HTTPTransportBoringSSL>(new HTTPTransportBoringSSL);
}

}  // namespace crashpad
