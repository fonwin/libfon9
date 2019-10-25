// \file f9twf/ExgMapSessionDn.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMapSessionDn_hpp__
#define __f9twf_ExgMapSessionDn_hpp__
#include "f9twf/ExgMapFcmId.hpp"
#include "f9twf/ExgLineTmpArgs.hpp"

namespace f9twf {

/// 期交所 P07、PA7: SessionId => dn:port 對照表.
class f9twf_API ExgMapSessionDn {
   using ExgSessionKey = uint32_t;
   static_assert(sizeof(ExgSessionKey) >= sizeof(FcmId) + sizeof(SessionId),
                 "ExgSessionKey too small.");
   using SesToDn = fon9::SortedVector<ExgSessionKey, std::string>;
   SesToDn  SesToDn_;

public:
   void reserve(size_t sz) {
      this->SesToDn_.reserve(sz);
   }
   void shrink_to_fit() {
      this->SesToDn_.shrink_to_fit();
   }
   void swap(ExgMapSessionDn& rhs) {
      this->SesToDn_.swap(rhs.SesToDn_);
   }

   /// 載入期交所P07、PA7.
   void LoadP07(const ExgMapBrkFcmId& mapFcmId, fon9::StrView fctx);

   /// 設定一筆(一行) dn, 使用 P07 格式.
   bool FeedLine(const ExgMapBrkFcmId& mapFcmId, fon9::StrView ctxln);

   /// 在 devcfg 尾端加上 "|dn=...";
   bool AppendDn(const ExgLineTmpArgs& lineArgs, std::string& devcfg) const;
};

} // namespaces
#endif//__f9twf_ExgMapSessionDn_hpp__
