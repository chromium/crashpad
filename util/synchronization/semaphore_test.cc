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

#include "util/synchronization/semaphore.h"

#include <pthread.h>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(Semaphore, Simple) {
  Semaphore semaphore(1);
  semaphore.Wait();
  semaphore.Signal();
}

struct ThreadMainInfo {
  pthread_t pthread;
  Semaphore* semaphore;
  size_t iterations;
};

void* ThreadMain(void* argument) {
  ThreadMainInfo* info = reinterpret_cast<ThreadMainInfo*>(argument);
  for (size_t iteration = 0; iteration < info->iterations; ++iteration) {
    info->semaphore->Wait();
  }
  return NULL;
}

TEST(Semaphore, Threaded) {
  Semaphore semaphore(0);
  ThreadMainInfo info;
  info.semaphore = &semaphore;
  info.iterations = 1;

  int rv = pthread_create(&info.pthread, NULL, ThreadMain, &info);
  ASSERT_EQ(0, rv) << "pthread_create";

  semaphore.Signal();

  rv = pthread_join(info.pthread, NULL);
  ASSERT_EQ(0, rv) << "pthread_join";
}

TEST(Semaphore, TenThreaded) {
  // This test has a smaller initial value (5) than threads contending for these
  // resources (10), and the threads each try to obtain the resource a different
  // number of times.
  Semaphore semaphore(5);
  const size_t kThreads = 10;
  ThreadMainInfo info[kThreads];
  size_t iterations = 0;
  int rv;
  for (size_t index = 0; index < kThreads; ++index) {
    info[index].semaphore = &semaphore;
    info[index].iterations = index;
    iterations += info[index].iterations;

    rv = pthread_create(&info[index].pthread, NULL, ThreadMain, &info[index]);
    ASSERT_EQ(0, rv) << "pthread_create";
  }

  for (size_t iteration = 0; iteration < iterations; ++iteration) {
    semaphore.Signal();
  }

  for (size_t index = 0; index < kThreads; ++index) {
    rv = pthread_join(info[index].pthread, NULL);
    ASSERT_EQ(0, rv) << "pthread_join";
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
