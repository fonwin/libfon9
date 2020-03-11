/// \file fon9/io/SocketAddress.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketAddress_hpp__
#define __fon9_io_SocketAddress_hpp__
#include "fon9/StrView.hpp"

fon9_BEFORE_INCLUDE_STD;
#ifdef fon9_WINDOWS
#include <WinSock2.h> // sockaddr_storage
#include <Ws2ipdef.h> // sockaddr_in6
#else// POSIX
#include <sys/socket.h>
#include <netinet/in.h>
#endif
fon9_AFTER_INCLUDE_STD;

namespace fon9 { namespace io {

using sa_family_t = decltype(static_cast<struct sockaddr*>(nullptr)->sa_family);
enum class AddressFamily : sa_family_t {
   /// unspecified
   UNSPEC = AF_UNSPEC,
   /// local to host (pipes, portals)
   UNIX = AF_UNIX,
   /// internetwork: UDP, TCP, etc.
   INET4 = AF_INET,
   /// Internetwork Version 6
   INET6 = AF_INET6,
};

/// \ingroup io
/// Socket IP 地址.
struct fon9_API SocketAddress {
   using port_t = decltype(static_cast<struct sockaddr_in*>(nullptr)->sin_port);
   union {
      /// 可用 Addr_.sa_family 判斷 AF_INET 或 AF_INET6;
      struct sockaddr         Addr_;
      /// 當 Addr_.sin_family = AF_INET 時, 使用 Addr4_: IPv4.
      struct sockaddr_in      Addr4_;
      /// 當 Addr_.sin_family = AF_INET6 時, 使用 Addr6_: IPv6.
      struct sockaddr_in6     Addr6_;
      /// 系統提供的 Addr 儲存機制.
      struct sockaddr_storage AddrStorage_;
   };

   /// 透過 strAddr 設定 IPv4 或 IPv6 地址.
   /// - 若 strAddr.Get1st() == '[' 則為 IPv6: "[IPv6 address]:port"
   /// - 否則為 IPv4: "a.b.c.d:port"
   /// - 若沒有 address 部分, 則 address 設為 any, 只設定 port, 例如:
   ///   - []:port
   ///   - port
   void FromStr(StrView strAddr);

   enum {
      /// AddrStr:Port 字串輸出的最大可能長度(包含EOS).
      /// - `"[INET6_ADDRSTRLEN]:port5"`
      /// - `"1                23456789"`
      kMaxAddrPortStrSize = INET6_ADDRSTRLEN + 10
   };

   /// 建立位址字串, 不含 port.
   /// \return 傳回字串長度.
   size_t ToAddrStr(char strbuf[], size_t bufsz) const;

   template <size_t bufsz>
   size_t ToAddrStr(char(&strbuf)[bufsz]) const {
      return this->ToAddrStr(strbuf, bufsz);
   }

   /// 建立位址字串, 包含 ":port"
   /// \return 傳回字串長度.
   size_t ToAddrPortStr(char strbuf[], size_t bufsz) const;

   template <size_t bufsz>
   size_t ToAddrPortStr(char(&strbuf)[bufsz]) const {
      return this->ToAddrPortStr(strbuf, bufsz);
   }

   /// 根據 family 取得 addr 實際占用的長度.
   int GetAddrLen() const {
      return static_cast<int>(this->Addr_.sa_family == AF_INET6 ? sizeof(this->Addr6_) : sizeof(this->Addr4_));
   }

   /// 取得 port, 自動判斷 AF_INET6 or AF_INET.
   port_t GetPort() const {
      return this->Addr_.sa_family == AF_INET6
         ? ntohs(this->Addr6_.sin6_port)
         : ntohs(this->Addr4_.sin_port);
   }

   AddressFamily GetAF() const {
      return static_cast<AddressFamily>(this->Addr_.sa_family);
   }

   /// 是否為 any addr?
   bool IsAddrAny() const;

   /// 是否為 any addr && port==0
   bool IsEmpty() const;

   bool IsUnspec() const {
      return this->Addr_.sa_family == AF_UNSPEC;
   }

   /// 檢查 ip, port 是否相同.
   bool operator==(const SocketAddress& rhs) const;
   bool operator!=(const SocketAddress& rhs) const {
      return !(*this == rhs);
   }
   /// 比較 addr 的大小, 不包含 port;
   /// 如果 sa_family 不同, 則返回 this->Addr_.sa_family - rhs.Addr_.sa_family;
   int CompareAddr(const SocketAddress& rhs) const;

   void SetAddrAny(AddressFamily af, port_t port);
};

enum {
   /// \ingroup io
   /// 一個 tcp connection address 字串輸出的最大可能長度.
   /// "R=kMaxAddrPortStrSize|L=kMaxAddrPortStrSize"
   /// "123                  456                  7"
   /// 額外 +8: 預留保險.
   kMaxTcpConnectionUID = (SocketAddress::kMaxAddrPortStrSize * 2) + 7 + 8
};

/// \ingroup io
/// 設定連線雙方的UniqueID字串, 格式如下:
///   - IPv4:  "R=a.b.c.d:port|L=a.b.c.d:port"
///   - IPv6:  "R=[...]:port|L=[...]:port"
/// 若 addrRemote==nullptr 則沒有 "R=..." 這段.
/// 若 addrLocal==nullptr 則沒有 "L=..." 這段.
extern fon9_API StrView MakeTcpConnectionUID(char* uid, size_t uidSize, const SocketAddress* addrRemote, const SocketAddress* addrLocal);

template <size_t uidSize>
inline StrView MakeTcpConnectionUID(char(&uid)[uidSize], const SocketAddress* addrRemote, const SocketAddress* addrLocal) {
   return MakeTcpConnectionUID(uid, uidSize, addrRemote, addrLocal);
}

struct SocketAddressRange {
   io::SocketAddress AddrFrom_;
   io::SocketAddress AddrTo_;

   /// 檢查 addr 是否在 [AddrFrom_ .. AddrTo_] 範圍內, 包含 AddrFrom_ .. AddrTo_;
   bool InRange(const io::SocketAddress& addr) const {
      if (addr.CompareAddr(this->AddrFrom_) < 0)
         return false;
      if(this->AddrTo_.Addr_.sa_family == AF_UNSPEC
         || addr.CompareAddr(this->AddrTo_) <= 0)
         return true;
      return false;
   }
};
/// - (AddrFrom_ == AddrTo_): AddrFrom_.ToAddrStr();
///   - e.g. "192.168.1.3"
/// - (AddrFrom_ != AddrTo_): AddrFrom_.ToAddrStr() - AddrTo_.ToAddrStr()
///   - e.g. "192.168.1.0 - 192.168.1.255"
fon9_API char* ToStrRev(char* pout, const SocketAddressRange& value);
constexpr size_t ToStrMaxWidth(const SocketAddressRange&) {
   return kMaxTcpConnectionUID;
}

fon9_API void StrTo(StrView str, SocketAddressRange& value);

} } // namespaces
#endif//__fon9_io_SocketAddress_hpp__
