/// \file fon9/ConfigUtils.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ConfigUtils_hpp__
#define __fon9_ConfigUtils_hpp__
#include "fon9/BitvArchive.hpp"

namespace fon9 {

enum class EnabledYN : char {
   /// 除了 Yes, 其餘皆為 No.
   Yes = 'Y'
};

inline size_t ToBitv(RevBuffer& rbuf, EnabledYN value) {
   return ToBitv(rbuf, value == EnabledYN::Yes);
}

inline void BitvTo(DcQueue& buf, EnabledYN& value) {
   bool b = (value == EnabledYN::Yes);
   BitvTo(buf, b);
   value = b ? EnabledYN::Yes : EnabledYN{};
}

inline EnabledYN NormalizeEnabledYN(EnabledYN v) {
   return toupper(static_cast<char>(v)) == static_cast<char>(EnabledYN::Yes) ? EnabledYN::Yes : EnabledYN{};
}

/// value == EnabledYN::Yes 輸出 *--pout = 'Y';
/// 或不輸出(直接返回 pout);
inline char* ToStrRev(char* pout, EnabledYN value) {
   if (value == EnabledYN::Yes)
      *--pout = 'Y';
   return pout;
}

/// 移除 str 前端空白, 然後檢查第1碼:
/// - str.empty()
///   - 否則 *endptr = str.begin();
///   - 返回 EnabledYN{};
/// - toupper(str.Get1st())=='N';
///   - 若 toupper(str)=="NO"; 則 *endptr = str.begin()+2;
///   - 否則 *endptr = str.begin()+1;
///   - 返回 EnabledYN{};
/// - toupper(str.Get1st())=='Y';
///   - 若 toupper(str)=="YES"; 則 *endptr = str.begin()+3;
///   - 否則 *endptr = str.begin()+1;
///   - 返回 EnabledYN::Yes;
/// - 其餘:
///   - 否則 *endptr = str.begin();
///   - 返回 defaultValue;
fon9_API EnabledYN StrTo(StrView str, EnabledYN defaultValue = EnabledYN{}, const char** endptr = nullptr);

} // namespaces
#endif//__fon9_ConfigUtils_hpp__
