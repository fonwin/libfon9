/// \file fon9/FlowCounter.cpp
/// \author fonwinz@gmail.com
#include "fon9/FlowCounter.hpp"

namespace fon9 {

ConfigParser::Result FlowCounterArgs::OnTagValue(StrView tag, StrView& value) {
   if (tag == "Fc") {
      this->FcCount_ = fon9::StrTo(&value, this->FcCount_);
      if (StrTrimHead(&value).Get1st() == '/') {
         value.SetBegin(value.begin() + 1);
         this->FcTimeMS_ = fon9::StrTo(&value, this->FcTimeMS_);
      }
      return value.empty() ? ConfigParser::Result::Success : ConfigParser::Result::EInvalidValue;
   }
   return ConfigParser::Result::EUnknownTag;
}

} // namespace
