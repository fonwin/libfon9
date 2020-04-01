// \file fon9/fmkt/MdRtStream.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdRtStream_hpp__
#define __fon9_fmkt_MdRtStream_hpp__
#include "fon9/fmkt/MdRtStreamInn.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 每個 symbol 擁有自己的 MdRtStream;
class fon9_API MdRtStream {
   fon9_NON_COPY_NON_MOVE(MdRtStream);

   using Subj = seed::UnsafeSeedSubjT<MdRtSubrSP>;

   Subj              Subj_;
   InnApf::StreamRW  RtStorage_;
   InnApf::SizeT     RtStorageSize_;
   DayTime           InfoTime_{DayTime::Null()};
   MdRtsKind         InfoTimeKind_{};
   uint32_t          LastTimeSnapshotBS_{};

   void Save(RevBufferList&& rts);

   seed::OpResult SubscribeStream(SubConn* pSubConn, seed::Tab& tabRt, SymbPodOp& op, StrView args, seed::FnSeedSubr&& subr);
   seed::OpResult UnsubscribeStream(SubConn subConn);

public:
   MdRtStreamInnMgr& InnMgr_;

   MdRtStream(MdRtStreamInnMgr& innMgr) : InnMgr_(innMgr) {
   }

   void OpenRtStorage(Symb& symb) {
      this->InnMgr_.RtOpen(this->RtStorage_, symb);
      this->RtStorageSize_ = this->RtStorage_.Size();
   }

   /// 應在設定好 symb 盤別 (TDay + TradingSessionId) 之後通知.
   /// 執行底下工作:
   ///   this->Publish();
   ///   this->OpenRtStorage();
   ///   this->Save(RtsPackType::TradingSessionSt);
   void SessionClear(SymbTree& tree, Symb& symb);

   /// 移除商品, 通常是因為商品下市.
   void BeforeRemove(SymbTree& tree, Symb& symb);

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
      (void)lk; assert(lk.owns_lock());
      return this->UnsubscribeStream(subConn);
   }

   /// 打包並發行:
   /// rtFldId + infoTime(Bitv,Null表示InfoTime沒變) + rts;
   void Publish(seed::Tree& tree, const StrView& keyText,
                RtsPackType pkType, DayTime infoTime, RevBufferList&& rts);
   void PublishUpdateBS(seed::Tree& tree, const StrView& keyText,
                        SymbBS& symbBS, RevBufferList&& rts);
};

} } // namespaces
#endif//__fon9_fmkt_MdRtStream_hpp__
