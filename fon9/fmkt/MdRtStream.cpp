// \file fon9/fmkt/MdRtStream.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtStream.hpp"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields MdRtStream::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(MdRtStream, InfoTime_, "InfoTime"));
   return flds;
}
seed::OpResult MdRtStream::SubscribeStream(SubConn* pSubConn,
                                           seed::Tab& tabRt,
                                           SymbPodOp& op,
                                           StrView args,
                                           seed::FnSeedSubr&& subr) {
   StrView decoderName = StrFetchNoTrim(args, ':');
   if (decoderName != "MdRts")
      return seed::OpResult::bad_subscribe_stream_args;
   if (0);// 解析 args; 回補參數? 訂閱Deal? 訂閱BS快照(+Interval?)? 訂閱BS異動?...
   this->Subj_.Subscribe(pSubConn, subr);

   struct SubscribeStreamOK : public seed::SeedNotifyArgs {
      fon9_NON_COPY_NON_MOVE(SubscribeStreamOK);
      using base = SeedNotifyArgs;
      SymbPodOp& PodOp_;
      SubscribeStreamOK(SymbPodOp& op, seed::Tab& tabRt)
         : base(*op.Sender_, &tabRt, op.KeyText_, nullptr, seed::SeedNotifyKind::SubscribeStreamOK)
         , PodOp_(op) {
      }
      void MakeGridView() const override {
         RevBufferList rbuf{256};
         size_t tabidx = this->Tree_.LayoutSP_->GetTabCount();
         while (tabidx > 0) {
            auto* tab = this->Tree_.LayoutSP_->GetTab(--tabidx);
            this->PodOp_.BeginRead(*tab, [&rbuf](const seed::SeedOpResult& res, const seed::RawRd* rd) {
               if (rd == nullptr)
                  return;
               auto fldidx = res.Tab_->Fields_.size();
               while (fldidx > 0) {
                  auto* fld = res.Tab_->Fields_.Get(--fldidx);
                  fld->CellToBitv(*rd, rbuf);
               }
               ToBitv(rbuf, unsigned_cast(res.Tab_->GetIndex()));
            });
         }
         this->CacheGV_ = BufferTo<std::string>(rbuf.MoveOut());
      }
   };
   subr(SubscribeStreamOK{op, tabRt});
   return seed::OpResult::no_error;
}
//--------------------------------------------------------------------------//
void MdRtStream::Save(RevBufferList&& rts) {
   (void)rts; if (0); // MdRtStream::Save(rts).
}
//--------------------------------------------------------------------------//
constexpr MdRtsKind RtsPackType2MdRtsKind[] = {
   MdRtsKind::Deal,
   MdRtsKind::BS,
   MdRtsKind::BS,
   MdRtsKind::BS,
   MdRtsKind::TradingSessionSt,
   MdRtsKind::Base | MdRtsKind::Ref,
};
static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::DealPack)]         == MdRtsKind::Deal, "");
static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::SnapshotBS)]       == MdRtsKind::BS, "");
static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::CalculatedBS)]     == MdRtsKind::BS, "");
static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::UpdateBS)]         == MdRtsKind::BS, "");
static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::TradingSessionSt)] == MdRtsKind::TradingSessionSt, "");
static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::BaseInfoTwf)]      == (MdRtsKind::Base | MdRtsKind::Ref), "");

constexpr MdRtsKind GetMdRtsKind(RtsPackType pkType) {
   return RtsPackType2MdRtsKind[cast_to_underlying(pkType)];
}

struct MdRtStream::NotifyArgs : public seed::SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(NotifyArgs);
   using base = seed::SeedNotifyArgs;
   const BufferNode* const Rts_;

   template <class... ArgsT>
   NotifyArgs(const BufferNode* rts, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , Rts_(rts) {
   }
   void MakeGridView() const override {
      this->CacheGV_ = BufferTo<std::string>(this->Rts_);
   }
};

void MdRtStream::SessionClear(SymbTree& tree, Symb& symb) {
   if (0);// Open MdRts File: Key=SymbId.
   RevBufferList rts{64};
   *rts.AllocPacket<uint8_t>() = symb.TradingSessionSt_;
   *rts.AllocPacket<char>() = symb.TradingSessionId_;
   PutBigEndian(rts.AllocPacket<uint32_t>(), symb.TDayYYYYMMDD_);
   this->Publish(tree, ToStrView(symb.SymbId_), RtsPackType::TradingSessionSt, DayTime::Null(), std::move(rts));
}

void MdRtStream::Publish(seed::Tree& tree, const StrView& keyText,
                         RtsPackType pkType, DayTime infoTime, RevBufferList&& rts) {
   if (this->InfoTime_ == infoTime)
      RevPutBitv(rts, fon9_BitvV_NumberNull);
   else {
      this->InfoTime_ = infoTime;
      ToBitv(rts, infoTime);
   }

   *rts.AllocPacket<uint8_t>() = cast_to_underlying(pkType);

   NotifyArgs e{rts.cfront(), tree, nullptr/*tab*/, keyText, GetMdRtsKind(pkType)};
   Subj_.Publish(e);
   this->Save(std::move(rts));
}

} } // namespaces
