// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RIEGELI_BASE_PARALLELISM_H_
#define RIEGELI_BASE_PARALLELISM_H_

#include <stddef.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>

namespace riegeli {
namespace internal {

// A thread pool with lazily created worker threads, without a thread count
// limit. Worker threads exit after being idle for one minute.
class ThreadPool {
 public:
  ThreadPool() {}

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  ~ThreadPool();

  void Schedule(std::function<void()> task);

 private:
  std::mutex mutex_;
  // All variables below are guarded by mutex_.
  bool exiting_ = false;
  size_t num_threads_ = 0;
  size_t num_idle_threads_ = 0;
  std::deque<std::function<void()>> tasks_;
  std::condition_variable has_work_;
  std::condition_variable workers_exited_;
};

ThreadPool& DefaultThreadPool();

}  // namespace internal
}  // namespace riegeli

#endif  // RIEGELI_BASE_PARALLELISM_H_
