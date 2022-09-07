/// \file fon9/io/SocketConfig.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SocketConfig.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace io {

inline static void SetOptionsDefaultAfterZero(SocketOptions& opts) {
#ifdef fon9_WINDOWS // Windows 使用 Overlapped IO 因此 SENDBUF 預設為 0;
   // 請參考 "fon9/io/win/IocpSocket.hpp" 的說明.
   opts.SO_SNDBUF_ = 0;
#else
   opts.SO_SNDBUF_ = -1;
#endif
   opts.SO_RCVBUF_ = -1;
   opts.TCP_NODELAY_ = 1;
}

void SocketOptions::SetDefaults() {
   ZeroStruct(this);
   SetOptionsDefaultAfterZero(*this);
}

void SocketConfig::SetDefaults() {
   ZeroStruct(this);
   SetOptionsDefaultAfterZero(this->Options_);
   this->AddrBind_.Addr_.sa_family = this->AddrRemote_.Addr_.sa_family = AF_UNSPEC;
}

void SocketConfig::SetAddrFamily() {
   if (this->AddrBind_.Addr_.sa_family == AF_UNSPEC)
      this->AddrBind_.Addr_.sa_family = this->AddrRemote_.Addr_.sa_family;
   else if (this->AddrRemote_.Addr_.sa_family == AF_UNSPEC)
      this->AddrRemote_.Addr_.sa_family = this->AddrBind_.Addr_.sa_family;
}

//--------------------------------------------------------------------------//

ConfigParser::Result SocketOptions::OnTagValue(StrView tag, StrView& value) {
   if (iequals(tag, "TcpNoDelay"))
      this->TCP_NODELAY_ = (toupper(static_cast<unsigned char>(value.Get1st())) != 'N');
   else if (iequals(tag, "SNDBUF"))
      this->SO_SNDBUF_ = StrTo(value, -1);
   else if (iequals(tag, "RCVBUF"))
      this->SO_RCVBUF_ = StrTo(value, -1);
   else if (iequals(tag, "ReuseAddr"))
      this->SO_REUSEADDR_ = (toupper(static_cast<unsigned char>(value.Get1st())) == 'Y');
   else if (iequals(tag, "ReusePort"))
      this->SO_REUSEPORT_ = (toupper(static_cast<unsigned char>(value.Get1st())) == 'Y');
   else if (iequals(tag, "Linger")) {
      if (toupper(static_cast<unsigned char>(value.Get1st())) == 'N') { // 開啟linger設定,但等候0秒.
         this->Linger_.l_onoff = 1;
         this->Linger_.l_linger = 0;
      }
   }
   else if (iequals(tag, "KeepAlive"))
      this->KeepAliveInterval_ = StrTo(value, int{});
   else if (iequals(tag, "IpTos"))
      this->IpTos_ = static_cast<uint8_t>(HIntStrTo(value));
   else if (iequals(tag, "Dscp"))
      this->IpTos_ = static_cast<uint8_t>(HIntStrTo(value) << 2);
   else
      return ConfigParser::Result::EUnknownTag;
   return ConfigParser::Result::Success;
}

ConfigParser::Result SocketConfig::OnTagValue(StrView tag, StrView& value) {
   if (iequals(tag, "Bind"))
      this->AddrBind_.FromStr(value);
   else if (iequals(tag, "Remote"))
      this->AddrRemote_.FromStr(value);
   else
      return this->Options_.OnTagValue(tag, value);
   return ConfigParser::Result::Success;
}

ConfigParser::Result SocketConfig::Parser::OnTagValue(StrView tag, StrView& value) {
   if (value.IsNull() && this->AddrDefault_ && !this->AddrDefault_->GetPort()) {
      this->AddrDefault_->FromStr(tag);
      this->AddrDefault_ = nullptr;
   }
   else
      return this->Owner_.OnTagValue(tag, value);
   return ConfigParser::Result::Success;
}

SocketConfig::Parser::~Parser() {
   this->Owner_.SetAddrFamily();
}

} } // namespaces
