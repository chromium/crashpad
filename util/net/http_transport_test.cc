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

#include "util/net/http_transport.h"

#include <stdint.h>
#include <string.h>

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/file/file_io.h"
#include "util/net/http_body.h"
#include "util/net/http_headers.h"
#include "util/net/http_multipart_builder.h"
#include "util/test/multiprocess_exec.h"

namespace crashpad {
namespace test {
namespace {

class HTTPTransportTestFixture : public MultiprocessExec {
 public:
  using RequestValidator =
      void(*)(HTTPTransportTestFixture*, const std::string&);

  HTTPTransportTestFixture(const HTTPHeaders& headers,
                           scoped_ptr<HTTPBodyStream> body_stream,
                           uint16_t http_response_code,
                           RequestValidator request_validator)
      : MultiprocessExec(),
        headers_(headers),
        body_stream_(body_stream.Pass()),
        response_code_(http_response_code),
        request_validator_(request_validator) {
    // TODO(rsesek): Use a more robust mechanism to locate testdata
    // <https://code.google.com/p/crashpad/issues/detail?id=4>.
    SetChildCommand("util/net/http_transport_test_server.py", nullptr);
  }

  const HTTPHeaders& headers() { return headers_; }

 private:
  void MultiprocessParent() override {
    // Use Logging*FD() instead of Checked*FD() so that the test can fail
    // gracefully with a gtest assertion if the child does not execute properly.

    // The child will write the HTTP server port number as a packed unsigned
    // short to stdout.
    uint16_t port;
    ASSERT_TRUE(LoggingReadFile(ReadPipeFD(), &port, sizeof(port)));

    // Then the parent will tell the web server what response code to send
    // for the HTTP request.
    ASSERT_TRUE(LoggingWriteFile(
        WritePipeFD(), &response_code_, sizeof(response_code_)));

    // Now execute the HTTP request.
    scoped_ptr<HTTPTransport> transport(HTTPTransport::Create());
    transport->SetMethod("POST");
    transport->SetURL(base::StringPrintf("http://127.0.0.1:%d/upload", port));
    for (const auto& pair : headers_) {
      transport->SetHeader(pair.first, pair.second);
    }
    transport->SetBodyStream(body_stream_.Pass());

    EXPECT_EQ(transport->ExecuteSynchronously(), (response_code_ == 200));

    // Read until the child's stdout closes.
    std::string request;
    char buf[32];
    ssize_t bytes_read;
    while ((bytes_read = ReadFile(ReadPipeFD(), buf, sizeof(buf))) != 0) {
      ASSERT_GE(bytes_read, 0);
      request.append(buf, bytes_read);
    }

    if (request_validator_)
      request_validator_(this, request);
  }

  HTTPHeaders headers_;
  scoped_ptr<HTTPBodyStream> body_stream_;
  uint16_t response_code_;
  RequestValidator request_validator_;
};

const char kMultipartFormData[] = "multipart/form-data";

void GetHeaderField(const std::string& request,
                    const std::string& header,
                    std::string* value) {
  size_t index = request.find(header);
  ASSERT_NE(std::string::npos, index);
  // Since the header is never the first line of the request, it should always
  // be preceded by a CRLF.
  EXPECT_EQ('\n', request[index - 1]);
  EXPECT_EQ('\r', request[index - 2]);

  index += header.length();
  EXPECT_EQ(':', request[index++]);
  // Per RFC 7230 §3.2, there can be one or more spaces or horizontal tabs.
  // For testing purposes, just assume one space.
  EXPECT_EQ(' ', request[index++]);

  size_t header_end = request.find('\r', index);
  ASSERT_NE(std::string::npos, header_end);

  *value = request.substr(index, header_end - index);
}

void GetMultipartBoundary(const std::string& request,
                          std::string* multipart_boundary) {
  std::string content_type;
  GetHeaderField(request, kContentType, &content_type);

  ASSERT_GE(content_type.length(), strlen(kMultipartFormData));
  size_t index = strlen(kMultipartFormData);
  EXPECT_EQ(kMultipartFormData, content_type.substr(0, index));

  EXPECT_EQ(';', content_type[index++]);

  size_t boundary_begin = content_type.find('=', index);
  ASSERT_NE(std::string::npos, boundary_begin);
  EXPECT_EQ('=', content_type[boundary_begin++]);
  if (multipart_boundary) {
    *multipart_boundary = content_type.substr(boundary_begin);
  }
}

const char kBoundaryEq[] = "boundary=";

void ValidFormData(HTTPTransportTestFixture* fixture,
                   const std::string& request) {
  std::string actual_boundary;
  GetMultipartBoundary(request, &actual_boundary);

  const auto& content_type = fixture->headers().find(kContentType);
  ASSERT_NE(fixture->headers().end(), content_type);

  size_t boundary = content_type->second.find(kBoundaryEq);
  ASSERT_NE(std::string::npos, boundary);
  std::string expected_boundary =
      content_type->second.substr(boundary + strlen(kBoundaryEq));
  EXPECT_EQ(expected_boundary, actual_boundary);

  size_t body_start = request.find("\r\n\r\n");
  ASSERT_NE(std::string::npos, body_start);
  body_start += 4;

  std::string expected = "--" + expected_boundary + "\r\n";
  expected += "Content-Disposition: form-data; name=\"key1\"\r\n\r\n";
  expected += "test\r\n";
  ASSERT_LT(body_start + expected.length(), request.length());
  EXPECT_EQ(expected, request.substr(body_start, expected.length()));

  body_start += expected.length();

  expected = "--" + expected_boundary + "\r\n";
  expected += "Content-Disposition: form-data; name=\"key2\"\r\n\r\n";
  expected += "--abcdefg123\r\n";
  expected += "--" + expected_boundary + "--\r\n";
  ASSERT_EQ(body_start + expected.length(), request.length());
  EXPECT_EQ(expected, request.substr(body_start));
}

TEST(HTTPTransport, ValidFormData) {
  HTTPMultipartBuilder builder;
  builder.SetFormData("key1", "test");
  builder.SetFormData("key2", "--abcdefg123");

  HTTPHeaders headers;
  headers.insert(builder.GetContentType());

  HTTPTransportTestFixture test(headers, builder.GetBodyStream(), 200,
      &ValidFormData);
  test.Run();
}

const char kTextPlain[] = "text/plain";

void ErrorResponse(HTTPTransportTestFixture* fixture,
                   const std::string& request) {
  std::string content_type;
  GetHeaderField(request, kContentType, &content_type);
  EXPECT_EQ(kTextPlain, content_type);
}

TEST(HTTPTransport, ErrorResponse) {
  HTTPMultipartBuilder builder;
  HTTPHeaders headers;
  headers[kContentType] = kTextPlain;
  HTTPTransportTestFixture test(headers, builder.GetBodyStream(),
      404, &ErrorResponse);
  test.Run();
}

const char kTextBody[] = "hello world";

void UnchunkedPlainText(HTTPTransportTestFixture* fixture,
                        const std::string& request) {
  std::string header_value;
  GetHeaderField(request, kContentType, &header_value);
  EXPECT_EQ(kTextPlain, header_value);

  GetHeaderField(request, kContentLength, &header_value);
  const auto& content_length = fixture->headers().find(kContentLength);
  ASSERT_NE(fixture->headers().end(), content_length);
  EXPECT_EQ(content_length->second, header_value);

  size_t body_start = request.rfind("\r\n");
  ASSERT_NE(std::string::npos, body_start);

  EXPECT_EQ(kTextBody, request.substr(body_start + 2));
}

TEST(HTTPTransport, UnchunkedPlainText) {
  scoped_ptr<HTTPBodyStream> body_stream(new StringHTTPBodyStream(kTextBody));

  HTTPHeaders headers;
  headers[kContentType] = kTextPlain;
  headers[kContentLength] = base::StringPrintf("%zu", strlen(kTextBody));

  HTTPTransportTestFixture test(headers, body_stream.Pass(), 200,
      &UnchunkedPlainText);
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
