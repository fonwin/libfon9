// \file f9tws/ExgLineArgs.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgLineArgs.hpp"

namespace f9tws {

void ExgLineArgs::Clear() {
   this->BrkId_.Clear(' ');
   this->SocketId_.Clear(' ');
   this->PassCode_ = static_cast<uint16_t>(-1);
}

template <class CharAryT>
static bool CopyToCharAry(CharAryT& dst, fon9::StrView& value) {
   if (value.size() != sizeof(dst))
      return false;
   memcpy(dst.Chars_, value.begin(), sizeof(dst));
   return true;
}

bool ExgLineArgs::Parse(fon9::StrView tag, fon9::StrView& value) {
   if (tag == "BrkId")
      return CopyToCharAry(this->BrkId_, value);
   if (tag == "SocketId" || tag == "PvcId")
      return CopyToCharAry(this->SocketId_, value);
   if (tag == "Pass") {
      this->PassCode_ = fon9::StrTo(&value, this->PassCode_);
      return value.empty();
   }
   value.Reset(nullptr);
   return false;
}

std::string ExgLineArgs::MakeErrMsg(fon9::StrView tag, fon9::StrView value) {
   if (value.IsNull())
      return tag.ToString("Unknown tag: ");
   return tag.ToString() + value.ToString(" unknown value: ");
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
