/// \file fon9/ThreadTools.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ThreadTools_hpp__
#define __fon9_ThreadTools_hpp__
#include "fon9/sys/Config.hpp"

fon9_BEFORE_INCLUDE_STD
#include <thread>
fon9_AFTER_INCLUDE_STD

namespace fon9 {

inline void JoinThread(std::thread& thr) {
   if (thr.joinable())
      thr.join();
}

template <class ThreadContainer>
inline void JoinThreads(ThreadContainer& thrs) {
   for (std::thread& thr : thrs)
      JoinThread(thr);
}

inline void JoinOrDetach(std::thread& thr) {
   if (thr.get_id() == std::this_thread::get_id())
      thr.detach();
   else
   if (thr.joinable())
      thr.join();
}

template <class ThreadContainer>
inline void JoinOrDetach(ThreadContainer& thrs) {
   for (std::thread& thr : thrs)
      JoinOrDetach(thr);
}

/// 設定 ThreadName.
/// 可讓 M$ VS 開發環境, 在 debug 時的 Threads 視窗, 清楚看出各個 threads 的用途.
#ifdef fon9_WINDOWS
fon9_API void SetCurrentThreadName(const char* threadName);
#else
inline void SetCurrentThreadName(const char* threadName) {
   (void)threadName;
}
#endif

} // namespace fon9
#endif//__fon9_ThreadTools_hpp__
