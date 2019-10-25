// \file f9twf/ExgLineTmpArgs.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgLineTmpArgs_hpp__
#define __f9twf_ExgLineTmpArgs_hpp__
#include "f9twf/ExgTmpTypes.hpp"
#include "fon9/ConfigParser.hpp"

namespace f9twf {

fon9_WARN_DISABLE_PADDING;
struct ExgLineTmpArgs {
   TmpApCode      ApCode_;
   TmpSessionId   SessionId_;
   TmpFcmId       SessionFcmId_;
   uint16_t       Pass_;
   bool           IsUseSymNum_;

   void Clear() {
      memset(this, 0, sizeof(*this));
   }
   fon9::ConfigParser::Result OnTagValue(fon9::StrView tag, fon9::StrView& value);
   std::string Verify() const;
};
fon9_WARN_POP;

/// - cfg = "FcmId=數字代號|SessionId=|Pass=|ApCode="; 每個欄位都必須提供.
/// - 額外的設定 "IsUseSymNum=Y";  送給期交所的 SymbId, 使用數字格式.
///   預設使用文字格式(根據 SymbolId 的長度決定使用 LongId or ShortId).
/// - retval.empty() 成功, retval = 失敗訊息.
f9twf_API std::string ExgLineTmpArgsParser(ExgLineTmpArgs& args, fon9::StrView cfg);

} // namespaces
#endif//__f9twf_ExgLineTmpArgs_hpp__
