// \file f9twf/ExgMapSessionDn.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMapSessionDn.hpp"

namespace f9twf {

bool ExgMapSessionDn::FeedLine(const ExgMapBrkFcmId& mapFcmId, fon9::StrView ctxln) {
   static const char kCstrSessionId[] = ".session";
   if (fon9::StrTrim(&ctxln).size() < sizeof(BrkId) + sizeof(kCstrSessionId))
      return false;
   BrkId brkid{ctxln.begin(), sizeof(BrkId)};
   for (char& c : brkid)
      c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
   ExgSessionKey seskey = mapFcmId.GetFcmId(brkid);
   const char* pfind = std::search(ctxln.begin(), ctxln.end(),
                                   kCstrSessionId, kCstrSessionId + sizeof(kCstrSessionId) - 1);
   if (pfind == ctxln.end())
      return false;
   seskey = (seskey << (sizeof(SessionId) * 8))
      + fon9::StrTo(fon9::StrView{pfind + sizeof(kCstrSessionId) - 1, ctxln.end()}, SessionId{});
   // 期交所資料: 
   //   f906000.test.session98.tmp.fut.taifex,30006
   //   f906000.test.session98.tmpbak1.fut.taifex,30006
   // TODO: fut/opt 會有相同的 SessionId 嗎?
   if ((pfind = ctxln.Find(',')) == nullptr)
      return false;
   std::string& dn = this->SesToDn_.kfetch(seskey).second;
   if (dn.empty())
      dn.append("|dn=");
   else
      dn.push_back(',');
   dn.append(ctxln.begin(), pfind);
   dn.push_back(':');
   dn.append(pfind + 1, ctxln.end());
   return true;
}
void ExgMapSessionDn::LoadP07(const ExgMapBrkFcmId& mapFcmId, fon9::StrView fctx) {
   this->SesToDn_.clear();
   while (fctx.empty())
      this->FeedLine(mapFcmId, fon9::StrFetchTrim(fctx, '\n'));
}

bool ExgMapSessionDn::AppendDn(const ExgLineTmpArgs& lineArgs, std::string& devcfg) const {
   ExgSessionKey seskey = TmpGetValueU(lineArgs.SessionFcmId_);
   seskey = (seskey << (sizeof(SessionId) * 8)) + TmpGetValueU(lineArgs.SessionId_);
   auto ifind = this->SesToDn_.find(seskey);
   if (ifind == this->SesToDn_.end())
      return false;
   devcfg.append(ifind->second);
   return true;
}

} // namespaces
