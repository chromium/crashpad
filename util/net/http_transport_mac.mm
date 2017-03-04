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

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Security.h>
#include <sys/utsname.h>

#include "base/auto_reset.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "package.h"
#include "third_party/apple_cf/CFStreamAbstract.h"
#include "util/file/file_io.h"
#include "util/misc/implicit_cast.h"
#include "util/net/http_body.h"

extern "C" {
CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA256Digest(
    SecCertificateRef certificate);
}  // extern "C"

namespace crashpad {

namespace {

NSString* AppendEscapedFormat(NSString* base,
                              NSString* format,
                              NSString* data) {
  return [base stringByAppendingFormat:
                   format,
                   [data stringByAddingPercentEncodingWithAllowedCharacters:
                             [[NSCharacterSet
                                 characterSetWithCharactersInString:
                                     @"()<>@,;:\\\"/[]?={} \t"] invertedSet]]];
}

// This builds the same User-Agent string that CFNetwork would build internally,
// but it uses PACKAGE_NAME and PACKAGE_VERSION in place of values obtained from
// the main bundle’s Info.plist.
NSString* UserAgentString() {
  NSString* user_agent = [NSString string];

  // CFNetwork would use the main bundle’s CFBundleName, or the main
  // executable’s filename if none.
  user_agent = AppendEscapedFormat(
      user_agent, @"%@", [NSString stringWithUTF8String:PACKAGE_NAME]);

  // CFNetwork would use the main bundle’s CFBundleVersion, or the string
  // “(unknown version)” if none.
  user_agent = AppendEscapedFormat(
      user_agent, @"/%@", [NSString stringWithUTF8String:PACKAGE_VERSION]);

  // Expected to be CFNetwork.
  NSBundle* nsurl_bundle = [NSBundle bundleForClass:[NSURLRequest class]];
  NSString* bundle_name = base::mac::ObjCCast<NSString>([nsurl_bundle
      objectForInfoDictionaryKey:base::mac::CFToNSCast(kCFBundleNameKey)]);
  if (bundle_name) {
    user_agent = AppendEscapedFormat(user_agent, @" %@", bundle_name);

    NSString* bundle_version = base::mac::ObjCCast<NSString>([nsurl_bundle
        objectForInfoDictionaryKey:base::mac::CFToNSCast(kCFBundleVersionKey)]);
    if (bundle_version) {
      user_agent = AppendEscapedFormat(user_agent, @"/%@", bundle_version);
    }
  }

  utsname os;
  if (uname(&os) != 0) {
    PLOG(WARNING) << "uname";
  } else {
    user_agent = AppendEscapedFormat(
        user_agent, @" %@", [NSString stringWithUTF8String:os.sysname]);
    user_agent = AppendEscapedFormat(
        user_agent, @"/%@", [NSString stringWithUTF8String:os.release]);

    // CFNetwork just uses the equivalent of os.machine to obtain the native
    // (kernel) architecture. Here, give the process’ architecture as well as
    // the native architecture. Use the same strings that the kernel would, so
    // that they can be de-duplicated.
#if defined(ARCH_CPU_X86)
    NSString* arch = @"i386";
#elif defined(ARCH_CPU_X86_64)
    NSString* arch = @"x86_64";
#else
#error Port
#endif
    user_agent = AppendEscapedFormat(user_agent, @" (%@", arch);

    NSString* machine = [NSString stringWithUTF8String:os.machine];
    if (![machine isEqualToString:arch]) {
      user_agent = AppendEscapedFormat(user_agent, @"; %@", machine);
    }

    user_agent = [user_agent stringByAppendingString:@")"];
  }

  return user_agent;
}

// An implementation of CFReadStream. This implements the V0 callback
// scheme.
class HTTPBodyStreamCFReadStream {
 public:
  explicit HTTPBodyStreamCFReadStream(HTTPBodyStream* body_stream)
      : body_stream_(body_stream) {
  }

  // Creates a new NSInputStream, which the caller owns.
  NSInputStream* CreateInputStream() {
    CFStreamClientContext context = {
      .version = 0,
      .info = this,
      .retain = nullptr,
      .release = nullptr,
      .copyDescription = nullptr
    };
    const CFReadStreamCallBacksV0 callbacks = {
      .version = 0,
      .open = &Open,
      .openCompleted = &OpenCompleted,
      .read = &Read,
      .getBuffer = &GetBuffer,
      .canRead = &CanRead,
      .close = &Close,
      .copyProperty = &CopyProperty,
      .schedule = &Schedule,
      .unschedule = &Unschedule
    };
    CFReadStreamRef read_stream = CFReadStreamCreate(nullptr,
        reinterpret_cast<const CFReadStreamCallBacks*>(&callbacks), &context);
    return base::mac::CFToNSCast(read_stream);
  }

 private:
  static HTTPBodyStream* GetStream(void* info) {
    return static_cast<HTTPBodyStreamCFReadStream*>(info)->body_stream_;
  }

  static Boolean Open(CFReadStreamRef stream,
                      CFStreamError* error,
                      Boolean* open_complete,
                      void* info) {
    *open_complete = TRUE;
    return TRUE;
  }

  static Boolean OpenCompleted(CFReadStreamRef stream,
                               CFStreamError* error,
                               void* info) {
    return TRUE;
  }

  static CFIndex Read(CFReadStreamRef stream,
                      UInt8* buffer,
                      CFIndex buffer_length,
                      CFStreamError* error,
                      Boolean* at_eof,
                      void* info) {
    if (buffer_length == 0) {
      *at_eof = FALSE;
      return 0;
    }

    FileOperationResult bytes_read =
        GetStream(info)->GetBytesBuffer(buffer, buffer_length);
    if (bytes_read < 0) {
      error->error = -1;
      error->domain = kCFStreamErrorDomainCustom;
    } else {
      *at_eof = bytes_read == 0;
    }

    return bytes_read;
  }

  static const UInt8* GetBuffer(CFReadStreamRef stream,
                                CFIndex max_bytes_to_read,
                                CFIndex* num_bytes_read,
                                CFStreamError* error,
                                Boolean* at_eof,
                                void* info) {
    return nullptr;
  }

  static Boolean CanRead(CFReadStreamRef stream, void* info) {
    return TRUE;
  }

  static void Close(CFReadStreamRef stream, void* info) {}

  static CFTypeRef CopyProperty(CFReadStreamRef stream,
                                CFStringRef property_name,
                                void* info) {
    return nullptr;
  }

  static void Schedule(CFReadStreamRef stream,
                       CFRunLoopRef run_loop,
                       CFStringRef run_loop_mode,
                       void* info) {}

  static void Unschedule(CFReadStreamRef stream,
                         CFRunLoopRef run_loop,
                         CFStringRef run_loop_mode,
                         void* info) {}

  HTTPBodyStream* body_stream_;  // weak

  DISALLOW_COPY_AND_ASSIGN(HTTPBodyStreamCFReadStream);
};

class HTTPTransportMac final : public HTTPTransport {
 public:
  HTTPTransportMac();
  ~HTTPTransportMac() override;

  bool ExecuteSynchronously(std::string* response_body) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPTransportMac);
};

}  // namespace
}  // namespace crashpad

@interface CrashpadSynchronousURLConnection
    : NSObject<NSURLConnectionDelegate, NSURLConnectionDataDelegate> {
 @private
  NSURLResponse** response_;
  NSError** error_;
  NSMutableData* responseBody_;
  BOOL running_;
  BOOL httpsPublicKeyPinningResult_;
}

- (NSData*)sendRequest:(NSURLRequest*)request
     returningResponse:(NSURLResponse**)response
                 error:(NSError**)error
                 owner:(crashpad::HTTPTransportMac*)owner;

// This is in NSURLConnectionDelegate, but re-declare it to avoid deprecation
// warnings when it’s called by this code.
- (BOOL)connection:(NSURLConnection*)connection
    canAuthenticateAgainstProtectionSpace:
        (NSURLProtectionSpace*)protectionSpace;

@end

@implementation CrashpadSynchronousURLConnection

- (NSData*)sendRequest:(NSURLRequest*)request
     returningResponse:(NSURLResponse**)response
                 error:(NSError**)error
                 owner:(crashpad::HTTPTransportMac*)owner {
  DCHECK(!response_);
  DCHECK(!error_);
  DCHECK(!responseBody_);
  DCHECK(!running_);
  DCHECK(!httpsPublicKeyPinningResult_);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  // Deprecated in OS X 10.11. The suggested replacement, NSURLSession, is only
  // available on 10.9 and later, and this needs to run on earlier releases.
  [NSURLConnection connectionWithRequest:request delegate:self];
#pragma clang diagnostic pop

  base::AutoReset<NSURLResponse**> resetResponse(&response_, response);
  base::AutoReset<NSError**> resetError(&error_, error);
  base::AutoReset<NSMutableData*> resetResponseBody(&responseBody_, nil);
  base::AutoReset<BOOL> reset_running(&running_, TRUE);
  base::AutoReset<BOOL> resetHttpsPublicKeyPinningResult(
      &httpsPublicKeyPinningResult_, FALSE);

  do {
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantFuture]];
  } while (running_);

  [*response autorelease];
  [*error autorelease];
  [responseBody_ autorelease];

  if (!httpsPublicKeyPinningResult_ &&
      [[*error domain] isEqualToString:NSURLErrorDomain] &&
      [*error code] == kCFURLErrorUserCancelledAuthentication) {
    NSMutableDictionary* userInfo =
        [NSMutableDictionary dictionaryWithDictionary:[*error userInfo]];
    [userInfo setObject:@"HTTPS public key pinning failure"
                 forKey:NSLocalizedDescriptionKey];
    *error = [NSError errorWithDomain:NSURLErrorDomain
                                 code:kCFURLErrorServerCertificateUntrusted
                             userInfo:userInfo];
  }

  return *error ? nil : responseBody_;
}

// NSURLConnectionDelegate:

- (BOOL)connection:(NSURLConnection*)connection
    canAuthenticateAgainstProtectionSpace:
        (NSURLProtectionSpace*)protectionSpace {
  return [[protectionSpace authenticationMethod]
      isEqualToString:NSURLAuthenticationMethodServerTrust];
}

- (void)connection:(NSURLConnection*)connection
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge {
  NSURLProtectionSpace* protectionSpace = [challenge protectionSpace];
  DCHECK([self connection:connection
      canAuthenticateAgainstProtectionSpace:protectionSpace]);

  SecTrustRef serverTrust = [protectionSpace serverTrust];

  // This doesn’t obtain or consider the SecTrustResultType that
  // SecTrustEvaluate() could set, which looks dangerous. But this
  // SecTrustEvaluate() call isn’t actually used to perform the customary checks
  // such as whether the leaf certificate’s common name or subject alternate
  // name matches the hostname. It’s only called as a precondition to calling
  // SecTrustGetCertificateAtIndex() to look for pinned keys. The customary
  // checks will be triggered if the pinning checks are satisfied when
  // -[id<NSURLAuthenticationChallengeSender>
  // performDefaultHandlingForAuthenticationChallenge:] is called.
  OSStatus status = SecTrustEvaluate(serverTrust, nullptr);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status) << "SecTrustEvaluate";
    [[challenge sender] cancelAuthenticationChallenge:challenge];
    return;
  }

  CFIndex certificateCount = SecTrustGetCertificateCount(serverTrust);
  for (CFIndex certificateIndex = 0;
       certificateIndex < certificateCount;
       ++certificateIndex) {
    SecCertificateRef certificate =
        SecTrustGetCertificateAtIndex(serverTrust, certificateIndex);
    if (!certificate) {
      LOG(ERROR) << "SecTrustGetCertificateAtIndex";
      continue;
    }

    base::ScopedCFTypeRef<CFDataRef> hash_data(
        SecCertificateCopySubjectPublicKeyInfoSHA256Digest(certificate));
    if (!hash_data.get()) {
      LOG(ERROR) << "SecCertificateCopySubjectPublicKeyInfoSHA256Digest";
      continue;
    }

    DCHECK_EQ(CFDataGetLength(hash_data.get()), 32);

    // Do something with hash_data.
  }

  // If not satisfied…
  if (false) {
    [[challenge sender] cancelAuthenticationChallenge:challenge];
    return;
  }

  // If satisfied…
  [[challenge sender]
      performDefaultHandlingForAuthenticationChallenge:challenge];
  httpsPublicKeyPinningResult_ = TRUE;
}

- (void)connection:(NSURLConnection*)connection
    didFailWithError:(NSError*)error {
  [*error_ autorelease];
  *error_ = [error retain];
  running_ = false;
}

// NSURLConnectionDataDelegate:

- (void)connection:(NSURLConnection*)connection
    didReceiveResponse:(NSURLResponse*)response {
  *response_ = [response retain];
}

- (void)connection:(NSURLConnection*)connection didReceiveData:(NSData*)data {
  if (!responseBody_) {
    responseBody_ = [[NSMutableData alloc] initWithData:data];
  } else {
    [responseBody_ appendData:data];
  }
}

- (void)connectionDidFinishLoading:(NSURLConnection*)connection {
  running_ = false;
}

@end

namespace crashpad {
namespace {

HTTPTransportMac::HTTPTransportMac() : HTTPTransport() {
}

HTTPTransportMac::~HTTPTransportMac() {
}

bool HTTPTransportMac::ExecuteSynchronously(std::string* response_body) {
  DCHECK(body_stream());

  @autoreleasepool {
    NSString* url_ns_string = base::SysUTF8ToNSString(url());
    NSURL* url = [NSURL URLWithString:url_ns_string];
    NSMutableURLRequest* request =
        [NSMutableURLRequest requestWithURL:url
                                cachePolicy:NSURLRequestUseProtocolCachePolicy
                            timeoutInterval:timeout()];
    [request setHTTPMethod:base::SysUTF8ToNSString(method())];

    // If left to its own devices, CFNetwork would build a user-agent string
    // based on keys in the main bundle’s Info.plist, giving ugly results if
    // there is no Info.plist. Provide a User-Agent string similar to the one
    // that CFNetwork would use, but with appropriate values in place of the
    // Info.plist-derived strings.
    [request setValue:UserAgentString() forHTTPHeaderField:@"User-Agent"];

    for (const auto& pair : headers()) {
      [request setValue:base::SysUTF8ToNSString(pair.second)
          forHTTPHeaderField:base::SysUTF8ToNSString(pair.first)];
    }

    HTTPBodyStreamCFReadStream body_stream_cf(body_stream());
    base::scoped_nsobject<NSInputStream> input_stream(
        body_stream_cf.CreateInputStream());
    [request setHTTPBodyStream:input_stream.get()];

    NSURLResponse* response = nil;
    NSError* error = nil;
    NSData* body =
        [[[[CrashpadSynchronousURLConnection alloc] init] autorelease]
                  sendRequest:request
            returningResponse:&response
                        error:&error
                        owner:this];

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

    if (response_body) {
      response_body->assign(static_cast<const char*>([body bytes]),
                            [body length]);
    }

    return true;
  }
}

}  // namespace

// static
std::unique_ptr<HTTPTransport> HTTPTransport::Create() {
  return std::unique_ptr<HTTPTransport>(new HTTPTransportMac());
}

}  // namespace crashpad
