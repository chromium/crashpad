// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "util/net/http_body_gzip.h"

#include <string.h>
#include <zlib.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/rand_util.h"
#include "gtest/gtest.h"
#include "util/net/http_body.h"

namespace crashpad {
namespace test {
namespace {

class ScopedZlibInflateStream {
 public:
  explicit ScopedZlibInflateStream(z_stream* zlib) : zlib_(zlib) {}
  ~ScopedZlibInflateStream() { EXPECT_EQ(Z_OK, inflateEnd(zlib_)); }

 private:
  z_stream* zlib_;
  DISALLOW_COPY_AND_ASSIGN(ScopedZlibInflateStream);
};

void GzipInflate(const std::string& compressed,
                 std::string* decompressed,
                 size_t buf_size) {
  decompressed->clear();

  if (buf_size == 0) {
    // There’s got to be some buffer.
    buf_size = 64;
  }

  std::unique_ptr<uint8_t[]> buf(new uint8_t[buf_size]);
  z_stream zlib = {};
  zlib.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(&compressed[0]));
  zlib.avail_in = compressed.size();
  zlib.next_out = buf.get();
  zlib.avail_out = buf_size;

  // Add this to inflateInit2()’s windowBits parameter for a gzip wrapper
  // instead of a zlib wrapper.
  const int kZlibGzipWrapper = 16;

  ASSERT_EQ(Z_OK, inflateInit2(&zlib, kZlibGzipWrapper + 0));
  ScopedZlibInflateStream zlib_inflate(&zlib);

  ASSERT_EQ(Z_STREAM_END, inflate(&zlib, Z_FINISH));

  ASSERT_LE(zlib.avail_out, buf_size);
  decompressed->assign(reinterpret_cast<char*>(buf.get()),
                       buf_size - zlib.avail_out);
}

void TestGzipDeflateInflate(const std::string& string) {
  std::unique_ptr<HTTPBodyStream> string_stream(
      new StringHTTPBodyStream(string));
  GzipHTTPBodyStream gzip_stream(std::move(string_stream));

  // Per http://www.zlib.net/zlib_tech.html, in the worst case, zlib will store
  // uncompressed data as-is, at an overhead of 5 bytes per 16384-byte block.
  // Add 20 bytes of overhead for the gzip wrapper per RFC 1952: a 10-byte
  // header and an 8-byte trailer, assuming no optional fields are present, plus
  // two extra bytes to account for zero-length input.
  size_t buf_size = string.size() + 20 + ((string.size() + 16383) / 16384) * 5;
  std::unique_ptr<uint8_t[]> buf(new uint8_t[buf_size]);
  FileOperationResult bytes = gzip_stream.GetBytesBuffer(buf.get(), buf_size);
  ASSERT_NE(bytes, -1);
  ASSERT_LE(static_cast<size_t>(bytes), buf_size);

  std::string compressed(reinterpret_cast<char*>(buf.get()), bytes);

  // Look for at least 18 bytes, the minimum size of the gzip wrapper’s header
  // and trailer.
  ASSERT_GE(compressed.size(), 18u);
  EXPECT_EQ(static_cast<char>(0x1f), compressed[0]);
  EXPECT_EQ(static_cast<char>(0x8b), compressed[1]);
  EXPECT_EQ(Z_DEFLATED, compressed[2]);

  std::string decompressed;
  ASSERT_NO_FATAL_FAILURE(
      GzipInflate(compressed, &decompressed, string.size()));

  EXPECT_EQ(string, decompressed);
}

std::string MakeString(size_t size) {
  std::string string;
  for (size_t i = 0; i < size; ++i) {
    string.append(1, i % 256);
  }
  return string;
}

TEST(GzipHTTPBodyStream, Empty) {
  TestGzipDeflateInflate(std::string());
}

TEST(GzipHTTPBodyStream, OneByte) {
  TestGzipDeflateInflate(std::string("Z"));
}

TEST(GzipHTTPBodyStream, FourKBytes_Deterministic) {
  TestGzipDeflateInflate(MakeString(4096));
}

TEST(GzipHTTPBodyStream, ManyBytes_Deterministic) {
  TestGzipDeflateInflate(MakeString(33333));
}

TEST(GzipHTTPBodyStream, FourKBytes_Random) {
  TestGzipDeflateInflate(base::RandBytesAsString(4096));
}

TEST(GzipHTTPBodyStream, ManyBytes_Random) {
  TestGzipDeflateInflate(base::RandBytesAsString(33333));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
