// \file fon9/DefaultThreadPool.cpp
// \author fonwinz@gmail.com
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/sys/OnWindowsMainExit.hpp"
#include "fon9/CyclicBarrier.hpp"

namespace fon9 {

struct DefaultThreadTaskHandler {
   using MessageType = DefaultThreadTask;
   DefaultThreadTaskHandler(DefaultThreadPool&) {
      if (gWaitLogSystemReady)
         gWaitLogSystemReady();
   }
   void OnMessage(DefaultThreadTask& task) {
      task();
   }
   void OnThreadEnd(const std::string& threadName) {
      (void)threadName;
   }
};

fon9_API DefaultThreadPool& GetDefaultThreadPool() {
   struct DefaultThreadPoolImpl : public DefaultThreadPool, sys::OnWindowsMainExitHandle {
      fon9_NON_COPY_NON_MOVE(DefaultThreadPoolImpl);
      DefaultThreadPoolImpl() {
         // TODO: 從 getenv()、argv 取得參數?
         // 目前有用到的地方:
         //  - log file(FileAppender)
         //  - DN resolve(io/SocketAddressDN.cpp)
         //  - Device 執行 OpQueue_
         //  - InnDbf
         //  - ...
         // 這裡的 threadCount 應該考慮以上不同工作種類的數量, 與 CPU 核心數無關.
         // 因為以上的工作都是: 低 CPU 用量, 且 IO blocking.
         uint32_t threadCount = 4;

         this->StartThread(threadCount, "fon9.DefaultThreadPool");
      }
      void OnWindowsMainExit_Notify() {
         this->NotifyForEndNow();
      }
      void OnWindowsMainExit_ThreadJoin() {
         this->WaitForEndNow();
      }
   };
   static DefaultThreadPoolImpl ThreadPool_;
   return ThreadPool_;
}

fon9_API void WaitDefaultThreadPoolQuit(DefaultThreadPool& thrPool, unsigned waitTimes, TimeInterval tiSleep) {
   if (thrPool.GetThreadState() < ThreadState::EndAfterWorkDone) {
      for (; waitTimes > 0; --waitTimes) {
         unsigned       L = static_cast<unsigned>(thrPool.GetThreadCount());
         CyclicBarrier  waiter{L + 1};
         for (; L > 0; --L) {
            thrPool.EmplaceMessage([&waiter]() {
               waiter.Wait();
            });
         }
         waiter.Wait();
         std::this_thread::sleep_for(tiSleep.ToDuration());
      }
   }
   thrPool.WaitForEndAfterWorkDone();

   // 把剩餘工作做完, 避免 memory leak.
   DefaultThreadTaskHandler handler(thrPool);
   thrPool.WaitForEndNow(handler);
}

} // namespaces
