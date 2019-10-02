// \file f9twf/ExgLineTmpArgs.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgLineTmpArgs.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace f9twf {

std::string ExgLineTmpArgs::Verify() const {
   if (TmpGetValue<TmpFcmId_t>(this->FcmId_) == 0)
      return "Unknown FcmId";
   if (TmpGetValue<TmpSessionId_t>(this->SessionId_) == 0)
      return "Unknown SessionId";
   if (this->Pass_ > 9999)
      return "Unknown Pass";
   return std::string{};
}
fon9::ConfigParser::Result ExgLineTmpArgs::OnTagValue(fon9::StrView tag, fon9::StrView& value) {
   if (tag == "ApCode")
      this->ApCode_ = static_cast<TmpApCode>(fon9::StrTo(&value, fon9::underlying_type_t<TmpApCode>{}));
   else if (tag == "SessionId")
      TmpPutValue(this->SessionId_, fon9::StrTo(&value, TmpSessionId_t{}));
   else if (tag == "FcmId")
      TmpPutValue(this->FcmId_, fon9::StrTo(&value, TmpFcmId_t{}));
   else if (tag == "Pass")
      this->Pass_ = fon9::StrTo(&value, this->Pass_);
   else
      return fon9::ConfigParser::Result::EUnknownTag;
   return value.empty() ? fon9::ConfigParser::Result::Success : fon9::ConfigParser::Result::EInvalidValue;
}

f9twf_API std::string ExgLineTmpArgsParser(ExgLineTmpArgs& args, fon9::StrView cfg) {
   return fon9::ParseFullConfig(args, cfg);
}

} // namespaces
