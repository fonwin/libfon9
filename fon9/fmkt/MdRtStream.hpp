// \file fon9/fmkt/MdRtStream.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdRtStream_hpp__
#define __fon9_fmkt_MdRtStream_hpp__
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/fmkt/Symb.hpp"
#include "fon9/seed/SeedSubr.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
class fon9_API MdRtStream {
   fon9_NON_COPY_NON_MOVE(MdRtStream);

   seed::UnsafeSeedSubj Subj_;
   DayTime  InfoTime_{DayTime::Null()};

   struct NotifyArgs;

   void Save(RevBufferList&& rts);

   seed::OpResult SubscribeStream(SubConn* pSubConn, seed::Tab& tabRt, SymbPodOp& op, StrView args, seed::FnSeedSubr&& subr);

public:
   MdRtStream() = default;

   /// 應在設定好 symb 新的盤別 (TDay + TradingSessionId) 之後通知.
   void SessionClear(SymbTree& tree, Symb& symb);

   DayTime InfoTime() const {
      return this->InfoTime_;
   }

   static seed::Fields MakeFields();

   template<class Locker>
   seed::OpResult SubscribeStream(const Locker& lk, SubConn* pSubConn, seed::Tab& tabRt, SymbPodOp& op, StrView args, seed::FnSeedSubr&& subr) {
      (void)lk; assert(lk.owns_lock());
      return this->SubscribeStream(pSubConn, tabRt, op, args, std::move(subr));
   }

   template<class Locker>
   seed::OpResult UnsubscribeStream(const Locker& lk, SubConn subConn) {
      return this->Subj_.SafeUnsubscribe(lk, subConn);
   }

   /// 打包並發行:
   /// rtFldId + infoTime(Bitv,Null表示InfoTime沒變) + rts;
   void Publish(seed::Tree& tree, const StrView& keyText,
                RtsPackType pkType, DayTime infoTime, RevBufferList&& rts);
};

} } // namespaces
#endif//__fon9_fmkt_MdRtStream_hpp__
