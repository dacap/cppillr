// Copyright (C) 2019  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef THREAD_POOL_H_INCLUDED
#define THREAD_POOL_H_INCLUDED

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>   // We want to replace base::thread with base::thread
#include <vector>

class thread_pool {
public:
  thread_pool(const size_t n)
    : m_running(true)
    , m_threads(n)
    , m_doingWork(0)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (size_t i=0; i<n; ++i)
      m_threads.push_back(std::move(std::thread([this]{ worker(); })));
  }

  ~thread_pool() {
    join_all();
  }

  void execute(std::function<void()>&& func) {
    assert(m_running);

    std::unique_lock<std::mutex> lock(m_mutex);
    m_work.push(std::move(func));
    m_cv.notify_one();
  }

  // Waits until the queue is empty.
  void wait_all() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cvWait.wait(lock, [this]() -> bool {
                          return
                            !m_running ||
                            (m_work.empty() && m_doingWork == 0);
                        });
  }

private:
  // Joins all threads without waiting the queue to be processed.
  void join_all() {
    m_running = false;
    m_cv.notify_all();

    for (auto& j : m_threads) {
      try {
        if (j.joinable())
          j.join();
      }
      catch (const std::exception& ex) {
        assert(false);
      }
      catch (...) {
        assert(false);
      }
    }
  }

  // Called for each worker thread.
  void worker() {
    while (m_running) {
      std::function<void()> func;
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]() -> bool {
                          return !m_running || !m_work.empty();
                        });
        if (!m_work.empty()) {
          func = std::move(m_work.front());
          ++m_doingWork;
          m_work.pop();
        }
      }
      try {
        if (func)
          func();
      }
      // TODO handle exceptions in a better way
      catch (const std::exception& e) {
        assert(false);
      }
      catch (...) {
        assert(false);
      }

      {
        std::unique_lock<std::mutex> lock(m_mutex);
        --m_doingWork;
        m_cvWait.notify_all();
      }
    }
  }

  std::atomic<bool> m_running;
  std::vector<std::thread> m_threads;
  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::condition_variable m_cvWait;
  std::queue<std::function<void()>> m_work;
  int m_doingWork;
};

#endif
