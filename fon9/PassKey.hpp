// \file fon9/PassKey.hpp
//
// PassIdMgr: 協助管理密碼設定
//    在系統的設定裡面使用 PassKey="PassId/Salt"; 可以避免在設定中出現[明碼].
//
// \author fonwinz@gmail.com
#ifndef __fon9_PassKey_hpp__
#define __fon9_PassKey_hpp__
#include "fon9/CharVector.hpp"

namespace fon9 {

typedef bool (*FnPassKeyToPassword)(const StrView& passKey, CharVector& password);

/// \ingroup Misc
/// 設定透過 passKey 取得 password 的功能.
extern fon9_API void SetFnPassKeyToPassword(FnPassKeyToPassword fnPassKeyToPassword);

/// \ingroup Misc
/// 透過 passKey 取得 password.
/// 預設: password = passKey, 返回 false;
extern fon9_API bool PassKeyToPassword(const StrView& passKey, CharVector& password);

/// \ingroup Misc
/// 透過 passKey 取得數字密碼.
/// \code
///   if (PassKeyToPassword()) return StrTo(password, defaultPassNum);
///   else                     return defaultPassNum;
/// \endcode
extern fon9_API unsigned PassKeyToPassNum(const StrView& passKey, unsigned defaultPassNum);

} // namespace
#endif//__fon9_PassKey_hpp__
