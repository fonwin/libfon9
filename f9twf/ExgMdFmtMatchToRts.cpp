// \file f9twf/ExgMdFmtMatchToRts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtMatchToRts.hpp"
#include "f9twf/ExgMdFmtMatch.hpp"
#include "f9twf/ExgMdFmtParsers.hpp"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

struct RtsMdMatchParser {
   fon9_NON_COPY_NON_MOVE(RtsMdMatchParser);
   using MdQty = uint32_t;
   fon9::RevBufferList  Rts_;
   ExgMdSymb&           Symb_;
   MdQty                SumQty_{};
   f9sv_DealFlag        Flags_;
   char                 Padding___[3];
   RtsMdMatchParser(unsigned pksz, ExgMdSymb& symb, bool isCalc)
      : Rts_{pksz}
      , Symb_(symb)
      , Flags_{isCalc ? f9sv_DealFlag_Calculated : f9sv_DealFlag{}} {
   }
   void OnParsedAMatch(const f9fmkt::PriQty& dst) {
      if (!IsEnumContains(this->Flags_, f9sv_DealFlag_Calculated))
         this->Symb_.CheckOHL(dst.Pri_, this->Symb_.Deal_.Data_.DealTime_);
      fon9::ToBitv(this->Rts_, dst.Qty_);
      fon9::ToBitv(this->Rts_, dst.Pri_);
      this->SumQty_ += static_cast<MdQty>(dst.Qty_);
   }
   void GetFirstMatch(f9fmkt::PriQty& dst, const ExgMdMatchHead& md) {
      md.FirstMatchPrice_.AssignTo(dst.Pri_, this->Symb_.PriceOrigDiv_);
      dst.Qty_ = fon9::PackBcdTo<MdQty>(md.FirstMatchQty_);
      this->OnParsedAMatch(dst);
   }
   void GetMatchData(f9fmkt::PriQty& dst, const ExgMdMatchData& md) {
      md.MatchPrice_.AssignTo(dst.Pri_, this->Symb_.PriceOrigDiv_);
      dst.Qty_ = fon9::PackBcdTo<MdQty>(md.MatchQty_);
      this->OnParsedAMatch(dst);
   }
   void CheckFieldChanged(f9sv_DealFlag flag, f9fmkt::Qty& dst, MdQty src) {
      if (dst == src)
         return;
      dst = src;
      this->Flags_ |= flag;
      fon9::ToBitv(this->Rts_, src);
   }
};

f9twf_API void I024MatchParserToRts(ExgMcMessage& e) {
   auto&       pk = *static_cast<const ExgMcI024*>(&e.Pk_);
   ExgMdLocker lk{e, pk.ProdId_};
   auto&       symb = *e.Symb_;
   // 由於解析成交明細時, 會同時檢查 High/Low, 並設定 High/Low 的時間,
   // 所以必須在解析成交明細前, 先取出成交時間.
   const auto  bfDealTime = symb.Deal_.Data_.DealTime_;
   symb.Deal_.Data_.InfoTime_ = pk.InformationTime_.ToDayTime();
   symb.Deal_.Data_.DealTime_ = pk.MatchTime_.ToDayTime();

   const uint8_t         count = static_cast<uint8_t>(pk.MatchDisplayItem_ & 0x7f);
   const ExgMdMatchData* pdat = pk.MatchData_ + count;
   const ExgMdMatchCnt*  pcnt = reinterpret_cast<const ExgMdMatchCnt*>(pdat);
   RtsMdMatchParser      parser(e.PkSize_, symb, (pk.CalculatedFlag_ == '1'));
   using MdQty = RtsMdMatchParser::MdQty;
   parser.CheckFieldChanged(f9sv_DealFlag_DealSellCntChanged, symb.Deal_.Data_.DealSellCnt_, fon9::PackBcdTo<MdQty>(pcnt->MatchSellCnt_));
   parser.CheckFieldChanged(f9sv_DealFlag_DealBuyCntChanged, symb.Deal_.Data_.DealBuyCnt_, fon9::PackBcdTo<MdQty>(pcnt->MatchBuyCnt_));
   // 由於 symb.Deal_ 沒有每次異動的事件通知, 所以只要填入最後一筆就好.
   // 但仍要將成交明細傳給 MdRtStream.
   if (count <= 0)
      parser.GetFirstMatch(symb.Deal_.Data_.Deal_, pk);
   else {
      parser.GetMatchData(symb.Deal_.Data_.Deal_, *--pdat);
      f9fmkt::PriQty pq;
      while (pdat != pk.MatchData_)
         parser.GetMatchData(pq, *--pdat);
      parser.GetFirstMatch(pq, pk);
   }
   *parser.Rts_.AllocPacket<uint8_t>() = count;

   const MdQty mTotQty = fon9::PackBcdTo<MdQty>(pcnt->MatchTotalQty_);
   if (IsEnumContains(parser.Flags_, f9sv_DealFlag_Calculated)) {
      assert(mTotQty == 0);
      parser.CheckFieldChanged(f9sv_DealFlag_TotalQtyLost, symb.Deal_.Data_.TotalQty_, 0);
   }
   else {
      parser.CheckFieldChanged(f9sv_DealFlag_TotalQtyLost, symb.Deal_.Data_.TotalQty_, mTotQty - parser.SumQty_);
   }
   symb.Deal_.Data_.TotalQty_ = mTotQty;

   if (symb.Deal_.Data_.DealTime_ != bfDealTime) {
      parser.Flags_ |= f9sv_DealFlag_DealTimeChanged;
      if (symb.Deal_.Data_.DealTime_ == symb.Deal_.Data_.InfoTime_)
         fon9::RevPutBitv(parser.Rts_, fon9_BitvV_NumberNull);
      else
         fon9::ToBitv(parser.Rts_, symb.Deal_.Data_.DealTime_);
   }
   *parser.Rts_.AllocPacket<uint8_t>() = fon9::cast_to_underlying(parser.Flags_);
   f9fmkt::PackMktSeq(parser.Rts_, symb.BS_.Data_.MarketSeq_, e.Channel_.GetChannelMgr()->Symbs_->CtrlFlags_, pk.ProdMsgSeq_);
   symb.MdRtStream_.Publish(ToStrView(symb.SymbId_), f9sv_RtsPackType_DealPack, f9sv_MdRtsKind_Deal,
                            symb.Deal_.Data_.InfoTime_, std::move(parser.Rts_));
}

} // namespaces
