// \file f9twf/ExgMdFmtParsers.hpp
//
// 這裡提供一些: 解析期交所行情訊息, 填入 Symbs 的範例.
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtParsers_hpp__
#define __f9twf_ExgMdFmtParsers_hpp__
#include "f9twf/ExgMcChannel.hpp"

namespace f9twf {

struct ExgMdLocker {
   fon9_NON_COPY_NON_MOVE(ExgMdLocker);
   ExgMdSymbs&          Symbs_;
   ExgMdSymbs::Locker   SymbsLocker_;

   ExgMdLocker(ExgMcMessage& e)
      : Symbs_(*e.Channel_.GetChannelMgr()->Symbs_)
      , SymbsLocker_{Symbs_.SymbMap_.Lock()} {
   }

   ExgMdLocker(ExgMcMessage& e, fon9::StrView symbId)
      : ExgMdLocker(e) {
      e.Symb_ = static_cast<ExgMdSymb*>(this->Symbs_.FetchSymb(this->SymbsLocker_, symbId).get());
   }

   template <class ProdId>
   ExgMdLocker(ExgMcMessage& e, const ProdId& prodId)
      : ExgMdLocker{e, fon9::StrView_eos_or_all(prodId.Chars_, ' ')} {
   }
};

f9twf_API void I010BasicInfoParser_V7(ExgMcMessage& e);
f9twf_API bool I010BasicInfoLockedParser_V7(ExgMcMessage& e, const ExgMdLocker&);
f9twf_API bool I010BasicInfoLockedParser_V8(ExgMcMessage& e, const ExgMdLocker&);
f9twf_API bool I012PriLmtsLockedParser(ExgMcMessage& e, const ExgMdLocker&);

f9twf_API void I011ContractParser(ExgMcMessage& e);
f9twf_API void I081BSParser(ExgMcMessage& e);
f9twf_API void I083BSParser(ExgMcMessage& e);
f9twf_API void I084SSParser(ExgMcMessage& e);

} // namespaces
#endif//__f9twf_ExgMdFmtParsers_hpp__
