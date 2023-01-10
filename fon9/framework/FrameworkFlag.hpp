/// \file fon9/framework/FrameworkFlag.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_FrameworkFlag_hpp__
#define __fon9_framework_FrameworkFlag_hpp__
#include "fon9/sys/Config.hpp"

namespace fon9 {

/// 預設為 false,
/// - 當 Framework::Initialize() 開始執行時設為 true;
/// - 在 Framework::Start() 返回前設為 false;
/// - 在有需要 MaAuth 的環境下(例: MaIo), 啟動前應判斷此旗標.
extern fon9_API bool IsFrameworkInitializing;

} // namespaces
#endif//__fon9_framework_FrameworkFlag_hpp__
