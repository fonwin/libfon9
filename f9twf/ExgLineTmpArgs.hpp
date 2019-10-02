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
   TmpFcmId       FcmId_;
   uint16_t       Pass_;

   void Clear() {
      memset(this, 0, sizeof(*this));
   }
   fon9::ConfigParser::Result OnTagValue(fon9::StrView tag, fon9::StrView& value);
   std::string Verify() const;
};
fon9_WARN_POP;

/// - cfg = "FcmId=數字代號|SessionId=|Pass=|ApCode=";
/// - 每個欄位都必須提供.
/// - retval.empty() 成功, retval = 失敗訊息.
f9twf_API std::string ExgLineTmpArgsParser(ExgLineTmpArgs& args, fon9::StrView cfg);

} // namespaces
#endif//__f9twf_ExgLineTmpArgs_hpp__
