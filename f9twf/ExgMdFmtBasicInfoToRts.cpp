// \file f9twf/ExgMdFmtBasicInfoToRts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtBasicInfoToRts.hpp"
#include "f9twf/ExgMdFmtBasicInfo.hpp"
#include "f9twf/ExgMdFmtParsers.hpp"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

struct BaseInfoKeeper {
   fon9_NON_COPY_NON_MOVE(BaseInfoKeeper);
   const uint32_t             PriceOrigDiv_;
   const uint32_t             StrikePriceDiv_;
   const SymbFlowGroup_t      FlowGroup_;
   const f9fmkt_TradingMarket TradingMarket_;
   char                       Padding___[2];

   BaseInfoKeeper(const ExgMdSymb& symb)
      : PriceOrigDiv_  {symb.PriceOrigDiv_}
      , StrikePriceDiv_{symb.StrikePriceDiv_}
      , FlowGroup_     {symb.FlowGroup_}
      , TradingMarket_ {symb.TradingMarket_} {
   }
   bool IsChanged(const ExgMdSymb& symb) const {
      return (this->PriceOrigDiv_   != symb.PriceOrigDiv_   // pk.DecimalLocator_
           || this->StrikePriceDiv_ != symb.StrikePriceDiv_ // pk.StrikePriceDecimalLocator_
           || this->FlowGroup_      != symb.FlowGroup_
           || this->TradingMarket_  != symb.TradingMarket_);
   }
   static void PackBaseInfo(fon9::RevBufferList& rbuf, const ExgMdSymb& symb, const ExgMdBasicInfo_C7& pk) {
      fon9::ToBitv(rbuf, symb.Ref_.Data_.PriRef_);
      *rbuf.AllocPacket<uint8_t>() = fon9::PackBcdTo<uint8_t>(pk.DecimalLocator_);
      *rbuf.AllocPacket<uint8_t>() = fon9::PackBcdTo<uint8_t>(pk.StrikePriceDecimalLocator_);
      *rbuf.AllocPacket<uint8_t>() = symb.FlowGroup_;
      *rbuf.AllocPacket<char>()    = symb.TradingMarket_;
   }
};
static void PackPriLmts(fon9::RevBufferList& rbuf, const TwfSymbRef::Data& priLmts) {
   const auto* const   plmtsBeg = priLmts.PriLmts_;
   const auto*         plmts = plmtsBeg + fon9::numofele(priLmts.PriLmts_);
   do {
      --plmts;
      if (!plmts->Dn_.IsNullOrZero() || !plmts->Up_.IsNullOrZero())
         break;
   } while (plmts != plmtsBeg);
   for (;;) {
      fon9::ToBitv(rbuf, plmts->Dn_);
      fon9::ToBitv(rbuf, plmts->Up_);
      if (plmts == plmtsBeg)
         break;
      --plmts;
   }
}

f9twf_API void I010BasicInfoParserToRts_V7(ExgMcMessage& e) {
   const auto&    pk = *static_cast<const ExgMcI010_V7*>(&e.Pk_);
   ExgMdLocker    lk{e, pk.ProdId_};
   auto&          symb = *e.Symb_;
   BaseInfoKeeper keeper{symb};

   const TwfSymbRef::Data  ref = symb.Ref_.Data_;
   if (!I010BasicInfoLockedParser_V7(e, lk))
      return;
   const bool isBaseChanged = keeper.IsChanged(symb);
   if (isBaseChanged || !ref.IsEuqalPriRefAndPriLmts(symb.Ref_.Data_)) {
      fon9::RevBufferList rbuf{128};
      PackPriLmts(rbuf, symb.Ref_.Data_);
      keeper.PackBaseInfo(rbuf, symb, pk);
      symb.MdRtStream_.Publish(ToStrView(symb.SymbId_),
                               f9sv_RtsPackType_BaseInfoTw,
                               f9sv_MdRtsKind_Base | f9sv_MdRtsKind_Ref,
                               e.Pk_.InformationTime_.ToDayTime(),
                               std::move(rbuf));
      if (isBaseChanged)
         symb.Contract_.OnSymbBaseChanged(symb, e.Channel_.GetChannelMgr());
   }
}

f9twf_API void I010BasicInfoParserToRts_V8(ExgMcMessage& e) {
   const auto&    pk = *static_cast<const ExgMcI010_V8*>(&e.Pk_);
   ExgMdLocker    lk{e, pk.ProdId_};
   auto&          symb = *e.Symb_;
   BaseInfoKeeper keeper{symb};
   const auto     priRef = symb.Ref_.Data_.PriRef_;
   if (!I010BasicInfoLockedParser_V8(e, lk))
      return;
   const bool isBaseChanged = keeper.IsChanged(symb);
   const bool isPriRefChanged = (priRef != symb.Ref_.Data_.PriRef_);
   if (isBaseChanged || isPriRefChanged) {
      fon9::RevBufferList rbuf{128};
      keeper.PackBaseInfo(rbuf, symb, pk);
      symb.MdRtStream_.Publish(ToStrView(symb.SymbId_),
                               f9sv_RtsPackType_BaseInfoTw,
                               isPriRefChanged ? (f9sv_MdRtsKind_Base | f9sv_MdRtsKind_Ref) : f9sv_MdRtsKind_Base,
                               e.Pk_.InformationTime_.ToDayTime(),
                               std::move(rbuf));
      if (isBaseChanged)
         symb.Contract_.OnSymbBaseChanged(symb, e.Channel_.GetChannelMgr());
   }
}
f9twf_API void I012BasePriLmts(ExgMcMessage& e) {
   const auto&    pk = *static_cast<const ExgMcI012*>(&e.Pk_);
   ExgMdLocker    lk{e, pk.ProdId_};
   auto&          symb = *e.Symb_;

   TwfSymbRef_Data::PriLmt priLmts[TwfSymbRef_Data::kPriLmtCount];
   memcpy(priLmts, symb.Ref_.Data_.PriLmts_, sizeof(priLmts));
   if (!I012PriLmtsLockedParser(e, lk))
      return;
   if (memcpy(priLmts, symb.Ref_.Data_.PriLmts_, sizeof(priLmts)) == 0)
      return;
   fon9::RevBufferList rbuf{128};
   PackPriLmts(rbuf, symb.Ref_.Data_);
   symb.MdRtStream_.Publish(ToStrView(symb.SymbId_),
                            f9sv_RtsPackType_PriLmts,
                            f9sv_MdRtsKind_Ref,
                            e.Pk_.InformationTime_.ToDayTime(),
                            std::move(rbuf));
}

} // namespaces
