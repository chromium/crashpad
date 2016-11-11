<!--
Copyright 2015 The Crashpad Authors. All rights reserved.

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

# Developing Crashpad

## Status

[Project status](status.md) information has moved to its own page.

## Introduction

Crashpad is a [Chromium project](https://dev.chromium.org/Home). Most of its
development practices follow Chromium’s. In order to function on its own in
other projects, Crashpad uses
[mini_chromium](https://chromium.googlesource.com/chromium/mini_chromium/), a
small, self-contained library that provides many of Chromium’s useful low-level
base routines. [mini_chromium’s
README](https://chromium.googlesource.com/chromium/mini_chromium/+/master/README.md)
provides more detail.

## Prerequisites

To develop Crashpad, the following tools are necessary, and must be present in
the `$PATH` environment variable:

 * Appropriate development tools. For macOS, this is
   [Xcode](https://developer.apple.com/xcode/) and for Windows, it’s [Visual
   Studio](https://www.visualstudio.com/).
 * Chromium’s
   [depot_tools](https://dev.chromium.org/developers/how-tos/depottools).
 * [Git](https://git-scm.com/). This is provided by Xcode on macOS and by
   depot_tools on Windows.
 * [Python](https://www.python.org/). This is provided by the operating system
   on macOS, and by depot_tools on Windows.

## Getting the Source Code

The main source code repository is a Git repository hosted at
https://chromium.googlesource.com/crashpad/crashpad. Although it is possible to
check out this repository directly with `git clone`, Crashpad’s dependencies are
managed by
[`gclient`](https://dev.chromium.org/developers/how-tos/depottools#TOC-gclient)
instead of Git submodules, so to work on Crashpad, it is best to use `fetch` to
get the source code.

`fetch` and `gclient` are part of the
[depot_tools](https://dev.chromium.org/developers/how-tos/depottools). There’s
no need to install them separately.

### Initial Checkout

```
$ mkdir ~/crashpad
$ cd ~/crashpad
$ fetch crashpad
```

`fetch crashpad` performs the initial `git clone` and `gclient sync`,
establishing a fully-functional local checkout.

### Subsequent Checkouts

```
$ cd ~/crashpad/crashpad
$ git pull -r
$ gclient sync
```

## Building

Crashpad uses [GYP](https://gyp.gsrc.io/) to generate
[Ninja](https://ninja-build.org/) build files. The build is described by `.gyp`
files throughout the Crashpad source code tree. The
[`build/gyp_crashpad.py`](https://chromium.googlesource.com/crashpad/crashpad/+/master/build/gyp_crashpad.py)
script runs GYP properly for Crashpad, and is also called when you run `fetch
crashpad`, `gclient sync`, or `gclient runhooks`.

The Ninja build files and build output are in the `out` directory. Both debug-
and release-mode configurations are available. The examples below show the debug
configuration. To build and test the release configuration, substitute `Release`
for `Debug`.

```
$ cd ~/crashpad/crashpad
$ ninja -C out/Debug
```

Ninja is part of the
[depot_tools](https://dev.chromium.org/developers/how-tos/depottools). There’s
no need to install it separately.

### Android

Crashpad’s Android port is in its early stages. This build relies on
cross-compilation. It’s possible to develop Crashpad for Android on any platform
that the [Android NDK (Native Development
Kit)](https://developer.android.com/ndk/) runs on.

If it’s not already present on your system, [download the NDK package for your
system](https://developer.android.com/ndk/downloads/) and expand it to a
suitable location. These instructions assume that it’s been expanded to
`~/android-ndk-r13`.

To build Crashpad, portions of the NDK must be reassembled into a [standalone
toolchain](https://developer.android.com/ndk/guides/standalone_toolchain.html).
This is a repackaged subset of the NDK suitable for cross-compiling for a single
Android architecture (such as `arm`, `arm64`, `x86`, and `x86_64`) targeting a
specific [Android API
level](https://source.android.com/source/build-numbers.html). The standalone
toolchain only needs to be built from the NDK one time for each set of options
desired. To build a standalone toolchain targeting 64-bit ARM and API level 21
(Android 5.0 “Lollipop”), run:

```
$ cd ~
$ python android-ndk-r13/build/tools/make_standalone_toolchain.py \
      --arch=arm64 --api=21 --install-dir=android-ndk-r13_arm64_api21
```

Note that Chrome uses Android API level 21 for 64-bit platforms and 16 for
32-bit platforms. See Chrome’s
[`build/config/android/config.gni`](https://chromium.googlesource.com/chromium/src/+/master/build/config/android/config.gni)
which sets `_android_api_level` and `_android64_api_level`.

To configure a Crashpad build for Android using this standalone toolchain, set
several environment variables directing the build to the standalone toolchain,
along with GYP options to identify an Android build. This must be done after any
`gclient sync`, or instead of any `gclient runhooks` operation. The environment
variables only need to be set for this `gyp_crashpad.py` invocation, and need
not be permanent.

```
$ cd ~/crashpad/crashpad
$ CC_target=~/android-ndk-r13_arm64_api21/bin/clang \
  CXX_target=~/android-ndk-r13_arm64_api21/bin/clang++ \
  AR_target=~/android-ndk-r13_arm64_api21/bin/aarch64-linux-android-ar \
  NM_target=~/android-ndk-r13_arm64_api21/bin/aarch64-linux-android-nm \
  READELF_target=~/android-ndk-r13_arm64_api21/bin/aarch64-linux-android-readelf \
  python build/gyp_crashpad.py \
      -DOS=android -Dtarget_arch=arm64 -Dclang=1 \
      --generator-output=out_android_arm64_api21 -f ninja-android
```

It is also possible to use GCC instead of Clang by making the appropriate
substitutions: `aarch64-linux-android-gcc` for `CC_target`;
`aarch64-linux-android-g++` for `CXX_target`; and `-Dclang=0` as an argument to
`gyp_crashpad.py`.

Target “triplets” to use for `ar`, `nm`, `readelf`, `gcc`, and `g++` are:

| Architecture | Target “triplet”        |
|:-------------|:------------------------|
| `arm`        | `arm-linux-androideabi` |
| `arm64`      | `aarch64-linux-android` |
| `x86`        | `i686-linux-android`    |
| `x86_64`     | `x86_64-linux-android`  |

The port is incomplete, but targets known to be working include `crashpad_util`,
`crashpad_test`, and `crashpad_test_test`. This list will grow over time. To
build, direct `ninja` to the specific `out` directory chosen by
`--generator-output` above.

```
$ ninja -C out_android_arm64_api21/out/Debug crashpad_test_test
```

## Testing

Crashpad uses [Google Test](https://github.com/google/googletest/) as its
unit-testing framework, and some tests use [Google
Mock](https://github.com/google/googletest/tree/master/googlemock/) as well. Its
tests are currently split up into several test executables, each dedicated to
testing a different component. This may change in the future. After a successful
build, the test executables will be found at `out/Debug/crashpad_*_test`.

```
$ cd ~/crashpad/crashpad
$ out/Debug/crashpad_minidump_test
$ out/Debug/crashpad_util_test
```

A script is provided to run all of Crashpad’s tests. It accepts a single
argument that tells it which configuration to test.

```
$ cd ~/crashpad/crashpad
$ python build/run_tests.py Debug
```

### Android

To test on Android, use [ADB (Android Debug
Bridge)](https://developer.android.com/studio/command-line/adb.html) to `adb
push` test executables and test data to a device or emulator, then use `adb
shell` to get a shell to run the test executables from. ADB is part of the
[Android SDK](https://developer.android.com/sdk/). Note that it is sufficient to
install just the command-line tools. The entire Android Studio IDE is not
necessary to obtain ADB.

This example runs `crashpad_test_test` on a device. This test executable has a
run-time dependency on a second executable and a test data file, which are also
transferred to the device prior to running the test.

```
$ cd ~/crashpad/crashpad
$ adb push out_android_arm64_api21/out/Debug/crashpad_test_test /data/local/tmp/
[100%] /data/local/tmp/crashpad_test_test
$ adb push \
      out_android_arm64_api21/out/Debug/crashpad_test_test_multiprocess_exec_test_child \
      /data/local/tmp/
[100%] /data/local/tmp/crashpad_test_test_multiprocess_exec_test_child
$ adb shell mkdir -p /data/local/tmp/crashpad_test_data_root/test
$ adb push test/paths_test_data_root.txt \
      /data/local/tmp/crashpad_test_data_root/test/
[100%] /data/local/tmp/crashpad_test_data_root/test/paths_test_data_root.txt
$ adb shell
device:/ $ cd /data/local/tmp
device:/data/local/tmp $ CRASHPAD_TEST_DATA_ROOT=crashpad_test_data_root \
                         ./crashpad_test_test
```

## Contributing

Crashpad’s contribution process is very similar to [Chromium’s contribution
process](https://dev.chromium.org/developers/contributing-code).

### Code Review

A code review must be conducted for every change to Crashpad’s source code. Code
review is conducted on [Chromium’s
Gerrit](https://chromium-review.googlesource.com/) system, and all code reviews
must be sent to an appropriate reviewer, with a Cc sent to
[crashpad-dev](https://groups.google.com/a/chromium.org/group/crashpad-dev). The
[`codereview.settings`](https://chromium.googlesource.com/crashpad/crashpad/+/master/codereview.settings)
file specifies this environment to `git-cl`.

`git-cl` is part of the
[depot_tools](https://dev.chromium.org/developers/how-tos/depottools). There’s
no need to install it separately.

```
$ cd ~/crashpad/crashpad
$ git checkout -b work_branch origin/master
…do some work…
$ git add …
$ git commit
$ git cl upload
```

The [PolyGerrit interface](https://polygerrit.appspot.com/) to Gerrit,
undergoing active development, is recommended. To switch from the classic
GWT-based Gerrit UI to PolyGerrit, click the PolyGerrit link in a Gerrit review
page’s footer.

Uploading a patch to Gerrit does not automatically request a review. You must
select a reviewer on the Gerrit review page after running `git cl upload`. This
action notifies your reviewer of the code review request. If you have lost track
of the review page, `git cl issue` will remind you of its URL. Alternatively,
you can request review when uploading to Gerrit by using `git cl upload
--send-mail`.

Git branches maintain their association with Gerrit reviews, so if you need to
make changes based on review feedback, you can do so on the correct Git branch,
committing your changes locally with `git commit`. You can then upload a new
patch set with `git cl upload` and let your reviewer know you’ve addressed the
feedback.

### Landing Changes

After code review is complete and “Code-Review: +1” has been received from all
reviewers, project members can commit the patch themselves:

```
$ cd ~/crashpad/crashpad
$ git checkout work_branch
$ git cl land
```

Alternatively, patches can be committed by clicking the “Submit” button in the
Gerrit UI.

Crashpad does not currently have a [commit
queue](https://dev.chromium.org/developers/testing/commit-queue), so
contributors who are not project members will have to ask a project member to
commit the patch for them. Project members can commit changes on behalf of
external contributors by clicking the “Submit” button in the Gerrit UI.

### External Contributions

Copyright holders must complete the [Individual Contributor License
Agreement](https://developers.google.com/open-source/cla/individual) or
[Corporate Contributor License
Agreement](https://developers.google.com/open-source/cla/corporate) as
appropriate before any submission can be accepted, and must be listed in the
[`AUTHORS`](https://chromium.googlesource.com/crashpad/crashpad/+/master/AUTHORS)
file. Contributors may be listed in the
[`CONTRIBUTORS`](https://chromium.googlesource.com/crashpad/crashpad/+/master/CONTRIBUTORS)
file.

## Buildbot

The [Crashpad Buildbot](https://build.chromium.org/p/client.crashpad/) performs
automated builds and tests of Crashpad. Before checking out or updating the
Crashpad source code, and after checking in a new change, it is prudent to check
the Buildbot to ensure that “the tree is green.”
