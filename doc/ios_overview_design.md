<!--
Copyright 2021 The Crashpad Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# iOS Crashpad Overview Design

[TOC]

## iOS Limitations

Crashpad on other platforms capture exceptions out-of-process. The iOS sandbox,
however, restricts applications from delegating work to their own separate
processes. This limitation means Crashpad on iOS must combine the work of the
handler and the client to the same process as the main application.

### The Crashpad In-Process Handler

In-process handling comes with a number of limitations and difficulties. It is
not possible to catch the main Mach exception EXC_CRASH, so certain groups of
crashes cannot be captured. This includes some major ones, like out-of-memory
crashes. This also introduces difficulties in capturing all the relevant crash
data and writing the minidump, as the process itself is in an unsafe state.

The handler may not, for example:

 - Allocate memory
 - Use libc, or most any library call.

The handler may only:

 - Use audited syscalls.
 - access memory via vm_read.

Due to these limitations, it is not possible to write a minidump immediately
from the crash handler. Instead an intermediate dump is written when a handler
would normally write a minidump (such as during an exception or a forced dump
without crashing).  The intermediate dump file will be converted to a minidump
on the next run (or when the application decides it's safe to do so).

### The Crashpad IntermediateDump Format

Due to the limitations of in-process handling, an intermediate dump file is
written during exceptions. This file is streamed serially all the data necessary
to generate a on the next application start.

The file format is similar to binary JSON, supporting keyed properties, maps and
arrays.

 - Property [key, length, value pair tuple]
 - StartMap, StartArray [key]
 - EndMap, EndArray, EndDocument

Similar to JSON, maps can contain other maps, arrays and properties.

### The Crashpad In-Process Client

Other Crashpad platforms process and transmit minidumps out-of-process. On iOS,
this happens in-process, and ideally should happen as soon as possible on next
launch of the application. Applications have control over when intermediate
dumps are converted to minidumps and Crashpad database can being transmitting
pending reports.


* ProcessIntermediateDumps<br>
For performance reasons applications may choose the correct time to convert
intermediate dumps, as well as append metadata to the pending intermediate
dumps. After converting, a minidump will be written to the Crashpad database,
similar to how other platforms write a minidump on exception handling.  If
uploading is enabled, this dump will also be immediately uploaded.

* EnableUploading<br>
For similar reasons, applications may choose the correct time to begin uploading
pending reports, such as when ideal network conditions exist.  By default,
clients start with uploading disabled.  Applications should call this API when
it is determined that it is appropriate to do so (such as on a few seconds after
startup, or when network connectivety is appropriate).


