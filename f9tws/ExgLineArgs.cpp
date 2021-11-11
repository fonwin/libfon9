// \file f9tws/ExgLineArgs.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgLineArgs.hpp"

namespace f9tws {

void ExgLineArgs::Clear() {
   this->BrkId_.Clear(' ');
   this->SocketId_.Clear(' ');
   this->PassCode_ = static_cast<uint16_t>(-1);
   this->HbInterval_ = 0;
}

template <class CharAryT>
static fon9::ConfigParser::Result CopyToCharAry(CharAryT& dst, fon9::StrView& value) {
   if (value.size() != sizeof(dst))
      return fon9::ConfigParser::Result::EInvalidValue;
   memcpy(dst.Chars_, value.begin(), sizeof(dst));
   return fon9::ConfigParser::Result::Success;
}

fon9::ConfigParser::Result ExgLineArgs::OnTagValue(fon9::StrView tag, fon9::StrView& value) {
   if (tag == "BrkId")
      return CopyToCharAry(this->BrkId_, value);
   else if (tag == "SocketId" || tag == "PvcId")
      return CopyToCharAry(this->SocketId_, value);
   else if (tag == "Pass")
      this->PassCode_ = fon9::StrTo(&value, this->PassCode_);
   else if (tag == "HbInt")
      this->HbInterval_ = fon9::StrTo(&value, this->HbInterval_);
   else {
      return fon9::ConfigParser::Result::EUnknownTag;
   }
   return value.empty() ? fon9::ConfigParser::Result::Success : fon9::ConfigParser::Result::EInvalidValue;
}

std::string ExgLineArgs::Verify() const {
   if (fon9::isspace(this->BrkId_.Chars_[0]))
      return "Unknown BrkId";
   if (fon9::isspace(this->SocketId_.Chars_[0]))
      return "Unknown SocketId/PvcId";
   if (this->PassCode_ > 9999)
      return "Unknown Pass";
   return std::string{};
}

} // namespaces
