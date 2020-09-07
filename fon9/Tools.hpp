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

/// \ingroup Misc
/// 將 big5 字串轉成 utf8.
/// 轉換後 outbuf 尾端不包含 EOS.
fon9_API size_t Big5ToUtf8NoEOS(StrView strBig5, char* outbuf, size_t outbufsz);
/// \ingroup Misc
/// 將 big5 字串轉成 utf8.
/// 轉換後 outbuf 尾端如果還有空間, 則會填入一個 EOS.
inline size_t Big5ToUtf8EOS(const StrView& strBig5, char* outbuf, size_t outbufsz) {
   size_t retval = Big5ToUtf8NoEOS(strBig5, outbuf, outbufsz);
   if (retval < outbufsz)
      outbuf[retval] = '\0';
   return retval;
}

} // namespace
#endif//__fon9_Tools_hpp__
