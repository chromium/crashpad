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

iOS has some limited capabilities that the rest of crashpad platforms support
requiring in-process handling of everything, with an in-between intermediate step.

No spawning of new processes, so handling must happen in process.

No EXC_CRASH, so some crashes (OOM) cannot be captured.


### The Crashpad In-Process Client

Runs in process, so must be respectful of main application.  Has similar API as
regular client, with a few extras.

APIs to to convert intermediate dumps to minidumps.

API to being using network and upload pending reports.


### The Crashpad IntermediateDump Format

Stream serially a bunch of bytes to the file,

Simple binary json-like format that allows quickly dumping, supporting keyed properties, maps and arrays.

File is broken into command, length value groups.

commands can be map (start, end), array (start, end), propery, and document_end

Properties are an opaque buffer of bytes with a length.

Arrays can only contains maps.

Maps can contain other maps, arrays and properties.


### The Crashpad In-Process Handler

In process handler must be able to write from within a broken process, so lots
of limitations.

  dont allocate memory

  don't want to talk to libc

  avoid library calls

  syscall basis only.

 information not in userspace
