// \file f9twf/ExgMdFmtBSToRts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtBSToRts.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "f9twf/ExgMdFmtParsers.hpp"
#include "fon9/BitvEncode.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace f9twf {
using namespace fon9;
using namespace fon9::fmkt;

f9twf_API void I081BSParserToRts(ExgMcMessage& e) {
   auto&       pk = *static_cast<const ExgMcI081*>(&e.Pk_);
   ExgMdLocker lk{e, pk.ProdId_};
   if (!e.Channel_.GetChannelMgr()->CheckSymbTradingSessionId(*e.Symb_))
      return;
   // ExgMdToUpdateBS(e.Pk_.InformationTime_.ToDayTime(), PackBcdTo<unsigned>(pk.NoMdEntries_), pk.MdEntry_, *e.Symb_);
   const auto  mdCount = PackBcdTo<uint8_t>(pk.NoMdEntries_);
   const auto* mdEntry = pk.MdEntry_;
   auto&       symbBS = e.Symb_->BS_.Data_;
   symbBS.InfoTime_ = e.Pk_.InformationTime_.ToDayTime();
   symbBS.Flags_ = f9sv_BSFlag{};
   uint8_t  uCount = 0;
   Pri      mdPri;

   // mdEntry 必須依序解析, 才能組成正確的委託簿.
   // 但填入 ToBitv() 必須是反向, 所以先暫時填入 xbuf, 然後從 xbuf 複製到 fnode;
   // 分配一個前端保留一小塊(sizeReserved)記憶體的 FwdBufferNode.
   // 前方保留的小塊記憶體: 可以將 node 轉成 RevBufferNode 之後使用.
   RevBufferFixedSize<sizeof(PriQty) + 8> xbuf;
   // 分配的記憶體量必須足夠: 最多 mdCount * (PQ 使用 Bitv 儲存所需的最大可能資料量 + sizeof(BSType)).
   FwdBufferNode* fnode = FwdBufferNode::AllocReserveFront(mdCount * (sizeof(PriQty) + 5) + 64, 64);
   RevBufferList  rbuf{16};
   auto*          fnodeEnd = fnode->GetDataEnd();
   rbuf.PushFront(fnode);
   for (uint8_t mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      PriQty*  dst;
      unsigned dstCount;
      uint8_t  bsType;
      switch (mdEntry->EntryType_) {
      case '0':
         bsType = cast_to_underlying(RtBSType::OrderBuy);
         symbBS.Flags_ |= f9sv_BSFlag_OrderBuy;
         dst = symbBS.Buys_;
         dstCount = symbBS.kBSCount;
         break;
      case '1':
         bsType = cast_to_underlying(RtBSType::OrderSell);
         symbBS.Flags_ |= f9sv_BSFlag_OrderSell;
         dst = symbBS.Sells_;
         dstCount = symbBS.kBSCount;
         break;
      case 'E':
         bsType = cast_to_underlying(RtBSType::DerivedBuy);
         symbBS.Flags_ |= f9sv_BSFlag_DerivedBuy;
         dst = &symbBS.DerivedBuy_;
         dstCount = 1;
         break;
      case 'F':
         bsType = cast_to_underlying(RtBSType::DerivedSell);
         symbBS.Flags_ |= f9sv_BSFlag_DerivedSell;
         dst = &symbBS.DerivedSell_;
         dstCount = 1;
         break;
      default:
         continue;
      }
      if (lv >= dstCount)
         continue;
      assert(lv <= 0x0f);
      bsType = static_cast<uint8_t>(bsType | lv);
      dst += lv;
      xbuf.Rewind();
      switch (mdEntry->UpdateAction_) {
      case '2': // Delete.
         bsType = static_cast<uint8_t>(bsType | cast_to_underlying(RtBSAction::Delete));
         if (auto mvc = dstCount - lv - 1)
            memmove(dst, dst + 1, mvc * sizeof(*dst));
         fon9::ForceZeroNonTrivial(dst + dstCount - lv - 1);
         break;
      case '0': // New.
         bsType = static_cast<uint8_t>(bsType | cast_to_underlying(RtBSAction::New));
         if (auto mvc = dstCount - lv - 1)
            memmove(dst + 1, dst, mvc * sizeof(*dst));
         ToBitv(xbuf, dst->Qty_ = PackBcdTo<uint32_t>(mdEntry->Qty_));
         mdEntry->Price_.AssignTo(dst->Pri_, e.Symb_->PriceOrigDiv_);
         ToBitv(xbuf, dst->Pri_);
         break;
      case '1': // Change.
      case '5': // Overlay.
         ToBitv(xbuf, dst->Qty_ = PackBcdTo<uint32_t>(mdEntry->Qty_));
         mdEntry->Price_.AssignTo(mdPri, e.Symb_->PriceOrigDiv_);
         if (mdPri == dst->Pri_)
            bsType = static_cast<uint8_t>(bsType | cast_to_underlying(RtBSAction::ChangeQty));
         else {
            bsType = static_cast<uint8_t>(bsType | cast_to_underlying(RtBSAction::ChangePQ));
            ToBitv(xbuf, dst->Pri_ = mdPri);
         }
         break;
      default:
         continue;
      }
      ++uCount;
      const auto sz = xbuf.GetUsedSize();
      *fnodeEnd = bsType;
      memcpy(fnodeEnd + 1, xbuf.GetCurrent(), sz);
      fnodeEnd += sz + 1;
   }
   fnode->SetDataEnd(fnodeEnd);
   // MSB 1 bit = Calculated?
   // 但是 ExgMcI081, UpdateBS 沒有 Calculated 旗標.
   assert(0 < uCount && uCount <= 0x80);
   if (uCount <= 0)
      return;
   *rbuf.AllocPacket<uint8_t>() = static_cast<uint8_t>(uCount - 1);
   if (IsEnumContains(e.Channel_.GetChannelMgr()->Symbs_->CtrlFlags_, MdSymbsCtrlFlag::HasChannelSeq))
      fon9::ToBitv(rbuf, symbBS.ChannelSeq_ = pk.GetChannelSeq());
   symbBS.MarketSeq_ = PackBcdToMarketSeq(pk.ProdMsgSeq_);
   e.Symb_->MdRtStream_.PublishUpdateBS(ToStrView(e.Symb_->SymbId_), symbBS, std::move(rbuf),
                                        e.Channel_.GetChannelMgr()->Symbs_->CtrlFlags_);
}
//--------------------------------------------------------------------------//
f9twf_API void I083BSParserToRts(ExgMcMessage& e) {
   auto&       pk = *static_cast<const ExgMcI083*>(&e.Pk_);
   ExgMdLocker lk{e, pk.ProdId_};
   if (!e.Channel_.GetChannelMgr()->CheckSymbTradingSessionId(*e.Symb_))
      return;
   ExgMdToSnapshotBS(e.Pk_.InformationTime_.ToDayTime(), PackBcdTo<unsigned>(pk.NoMdEntries_), pk.MdEntry_,
                     *e.Symb_, PackBcdToMarketSeq(pk.ProdMsgSeq_));
   auto&       symbBS = e.Symb_->BS_.Data_;
   const auto  pkType = (pk.CalculatedFlag_ == '1' ? f9sv_RtsPackType_CalculatedBS : f9sv_RtsPackType_SnapshotBS);
   if (pkType == f9sv_RtsPackType_CalculatedBS)
      symbBS.Flags_ |= f9sv_BSFlag_Calculated;
   RevBufferList rbuf{128};
   MdRtsPackSnapshotBS(rbuf, symbBS);
   PackMktSeq(rbuf, e.Channel_.GetChannelMgr()->Symbs_->CtrlFlags_, symbBS.MarketSeq_);
   e.Symb_->MdRtStream_.Publish(ToStrView(e.Symb_->SymbId_), pkType, f9sv_MdRtsKind_BS, symbBS.InfoTime_, std::move(rbuf));
}

} // namespaces
