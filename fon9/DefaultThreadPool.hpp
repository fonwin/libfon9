/// \file fon9/DefaultThreadPool.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_DefaultThreadPool_hpp__
#define __fon9_DefaultThreadPool_hpp__
#include "fon9/MessageQueue.hpp"

namespace fon9 {

/// \ingroup Thrs
/// 在 GetDefaultThreadPool() 裡面執行的單一作業。
using DefaultThreadTask = std::function<void()>;
struct DefaultThreadTaskHandler;
using DefaultThreadPool = MessageQueue<DefaultThreadTaskHandler, DefaultThreadTask>;

/// \ingroup Thrs
/// 取得 fon9 提供的一個 thread pool.
/// * 一般用於不急迫, 但比較花時間的簡單工作, 例如: 寫檔、domain name 查找...
/// * 程式結束時, 剩餘的工作會被拋棄!
fon9_API DefaultThreadPool& GetDefaultThreadPool();

/// \ingroup Thrs
/// 等候 thread pool 將所有的工作完成之後結束 thread pool.
/// 結束 thread pool 之後, 不會再處理後續加入的工作!!
/// 為了讓等候中的工作有機會全部做完, 所以:
/// - 總共等候 waitTimes 次循環(每個 thread 都完成工作).
/// - 每次循環之後睡一會兒(tiSleep) 等看看有沒有新工作.
fon9_API void WaitDefaultThreadPoolQuit(DefaultThreadPool& thrPool,
                                        unsigned waitTimes = 3,
                                        TimeInterval tiSleep = TimeInterval_Millisecond(10));

} // namespaces
#endif//__fon9_DefaultThreadPool_hpp__
