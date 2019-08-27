/// \file fon9/Tools.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Tools_hpp__
#define __fon9_Tools_hpp__
#include "fon9/Outcome.hpp"
#include "fon9/StrView.hpp"

namespace fon9 {

enum class HowWait {
   Unknown,
   Block,
   Yield,
   Busy,
};
inline bool IsBlockWait(HowWait value) {
   return value <= HowWait::Block;
}
fon9_API HowWait StrToHowWait(StrView value);
fon9_API StrView HowWaitToStr(HowWait value);

/// \ingroup Misc
/// 設定「現在 thread」的 CPU 綁定.
/// \retval Result3::kNoResult()    cpuAffinity < 0: 不綁定.
/// \retval Result3::kSuccess()     cpuAffinity >=0: 綁定成功.
/// \retval retval.IsError()        綁定失敗原因.
fon9_API Result3 SetCpuAffinity(int cpuAffinity);

} // namespace
#endif//__fon9_Tools_hpp__
