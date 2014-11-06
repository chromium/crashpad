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

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "util/net/http_body.h"

@interface CrashpadHTTPBodyStreamTransport : NSInputStream {
 @private
  NSStreamStatus streamStatus_;
  id<NSStreamDelegate> delegate_;
  crashpad::HTTPBodyStream* bodyStream_;  // weak
}
- (instancetype)initWithBodyStream:(crashpad::HTTPBodyStream*)bodyStream;
@end

@implementation CrashpadHTTPBodyStreamTransport

- (instancetype)initWithBodyStream:(crashpad::HTTPBodyStream*)bodyStream {
  if ((self = [super init])) {
    streamStatus_ = NSStreamStatusNotOpen;
    bodyStream_ = bodyStream;
  }
  return self;
}

// NSInputStream:

- (BOOL)hasBytesAvailable {
  // Per Apple's documentation: "May also return YES if a read must be attempted
  // in order to determine the availability of bytes."
  switch (streamStatus_) {
    case NSStreamStatusAtEnd:
    case NSStreamStatusClosed:
    case NSStreamStatusError:
      return NO;
    default:
      return YES;
  }
}

- (NSInteger)read:(uint8_t*)buffer maxLength:(NSUInteger)maxLen {
  streamStatus_ = NSStreamStatusReading;

  NSInteger rv = bodyStream_->GetBytesBuffer(buffer, maxLen);

  if (rv == 0)
    streamStatus_ = NSStreamStatusAtEnd;
  else if (rv < 0)
    streamStatus_ = NSStreamStatusError;
  else
    streamStatus_ = NSStreamStatusOpen;

  return rv;
}

- (BOOL)getBuffer:(uint8_t**)buffer length:(NSUInteger*)length {
  return NO;
}

// NSStream:

- (void)scheduleInRunLoop:(NSRunLoop*)runLoop
                  forMode:(NSString*)mode {
}

- (void)removeFromRunLoop:(NSRunLoop*)runLoop
                  forMode:(NSString*)mode {
}

- (void)open {
  streamStatus_ = NSStreamStatusOpen;
}

- (void)close {
  streamStatus_ = NSStreamStatusClosed;
}

- (NSStreamStatus)streamStatus {
  return streamStatus_;
}

- (id<NSStreamDelegate>)delegate {
  return delegate_;
}

- (void)setDelegate:(id)delegate {
  delegate_ = delegate;
}

@end

namespace crashpad {

namespace {

class HTTPTransportMac final : public HTTPTransport {
 public:
  HTTPTransportMac();
  ~HTTPTransportMac() override;

  bool ExecuteSynchronously() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPTransportMac);
};

HTTPTransportMac::HTTPTransportMac() : HTTPTransport() {
}

HTTPTransportMac::~HTTPTransportMac() {
}

bool HTTPTransportMac::ExecuteSynchronously() {
  DCHECK(body_stream());

  @autoreleasepool {
    NSString* url_ns_string = base::SysUTF8ToNSString(url());
    NSURL* url = [NSURL URLWithString:url_ns_string];
    NSMutableURLRequest* request =
        [NSMutableURLRequest requestWithURL:url
                                cachePolicy:NSURLRequestUseProtocolCachePolicy
                            timeoutInterval:timeout()];
    [request setHTTPMethod:base::SysUTF8ToNSString(method())];

    for (const auto& pair : headers()) {
      [request setValue:base::SysUTF8ToNSString(pair.second)
          forHTTPHeaderField:base::SysUTF8ToNSString(pair.first)];
    }

    base::scoped_nsobject<CrashpadHTTPBodyStreamTransport> transport(
        [[CrashpadHTTPBodyStreamTransport alloc] initWithBodyStream:
            body_stream()]);
    [request setHTTPBodyStream:transport.get()];

    NSURLResponse* response = nil;
    NSError* error = nil;
    [NSURLConnection sendSynchronousRequest:request
                          returningResponse:&response
                                      error:&error];

    if (error) {
      LOG(ERROR) << [[error localizedDescription] UTF8String] << " ("
                 << [[error domain] UTF8String] << " " << [error code] << ")";
      return false;
    }
    if (!response) {
      LOG(ERROR) << "no response";
      return false;
    }
    NSHTTPURLResponse* http_response =
        base::mac::ObjCCast<NSHTTPURLResponse>(response);
    if (!http_response) {
      LOG(ERROR) << "no http_response";
      return false;
    }
    NSInteger http_status = [http_response statusCode];
    if (http_status != 200) {
      LOG(ERROR) << base::StringPrintf("HTTP status %ld",
                                       implicit_cast<long>(http_status));
      return false;
    }

    return true;
  }
}

}  // namespace

// static
scoped_ptr<HTTPTransport> HTTPTransport::Create() {
  return scoped_ptr<HTTPTransport>(new HTTPTransportMac());
}

}  // namespace crashpad
