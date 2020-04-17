// \file fon9/fmkt/MdRtStream.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdRtStream_hpp__
#define __fon9_fmkt_MdRtStream_hpp__
#include "fon9/fmkt/MdRtStreamInn.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 每個 symbol 擁有自己的 MdRtStream;
class fon9_API MdRtStream : public SymbData {
   fon9_NON_COPY_NON_MOVE(MdRtStream);

   MdRtUnsafeSubj    UnsafeSubj_;
   InnApf::StreamRW  RtStorage_;
   InnApf::SizeT     RtStorageSize_;
   DayTime           InfoTime_{DayTime::Null()};
   MdRtsKind         InfoTimeKind_{};
   uint32_t          LastTimeSnapshotBS_{};

   void Save(RevBufferList&& rts);

   seed::OpResult SubscribeStream(SubConn* pSubConn, seed::Tab& tabRt, SymbPodOp& op, StrView args, seed::FnSeedSubr&& subr);
   seed::OpResult UnsubscribeStream(SubConn subConn) {
      return MdRtUnsafeSubj_UnsubscribeStream(this->UnsafeSubj_, subConn);
   }

   /// 在設定好 symb 盤別 (TDay + TradingSessionId) 之後通知.
   /// - 可能是 DailyClear, 或是 SessionClear; 都會來到此處.
   /// - 執行底下工作:
   ///   this->Publish();
   ///   this->OpenRtStorage();
   ///   this->Save(RtsPackType::TradingSessionId);
   void OnSessionChanged(const Symb& symb);

public:
   MdRtStreamInnMgr& InnMgr_;

   MdRtStream(MdRtStreamInnMgr& innMgr) : InnMgr_(innMgr) {
   }
   ~MdRtStream();

   void OpenRtStorage(Symb& symb) {
      this->InnMgr_.RtOpen(this->RtStorage_, symb);
      this->RtStorageSize_ = this->RtStorage_.Size();
   }

   /// 移除商品, 通常是因為商品下市.
   void BeforeRemove(SymbTree& tree, Symb& symb);
   void OnSymbDailyClear(SymbTree& tree, const Symb& symb) override;
   void OnSymbSessionClear(SymbTree& tree, const Symb& symb) override;

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
   /// pkType + infoTime(Bitv,Null表示InfoTime沒變) + rts;
   void Publish(const StrView& keyText, RtsPackType pkType, DayTime infoTime, RevBufferList&& rts);
   void PublishUpdateBS(const StrView& keyText, SymbBSData& symbBS, RevBufferList&& rts);

   /// 先把 pkType 打包進 rts: *rts.AllocPacket<uint8_t>() = cast_to_underlying(pkType);
   /// 然後: 發行 & 儲存.
   void PublishAndSave(const StrView& keyText, RtsPackType pkType, RevBufferList&& rts);
};
//--------------------------------------------------------------------------//
class fon9_API MdRtsNotifyArgs : public seed::SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(MdRtsNotifyArgs);
   using base = seed::SeedNotifyArgs;
public:
   const BufferNode* const RtsForGvStr_;
   MdRtsNotifyArgs(seed::Tree& tree, const StrView& keyText, MdRtsKind rtsKind, RevBufferList& rts)
      : base(tree, nullptr/*tab*/, keyText, rtsKind)
      , RtsForGvStr_{rts.cfront()} {
   }
   void MakeGridView() const override;
};

} } // namespaces
#endif//__fon9_fmkt_MdRtStream_hpp__
