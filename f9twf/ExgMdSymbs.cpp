// \file f9twf/ExgMdSymbs.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdSymbs.hpp"
#include "f9twf/ExgMdContracts.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9twf {

fon9::seed::LayoutSP ExgMdSymb::MakeLayout(bool isAddMarketSeq) {
   using namespace fon9;
   using namespace fon9::seed;
   using namespace fon9::fmkt;
   constexpr auto kTabFlag = TabFlag::NoSapling_NoSeedCommand_Writable;
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Base}, MakeFields(),                  kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Ref},  TwfSymbRef_MakeFields(),       kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BS},   SymbTwfBS_MakeFields(isAddMarketSeq, true), kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Deal}, SymbTwfDeal_MakeFields(isAddMarketSeq), kTabFlag}},
      f9fmkt_MAKE_TABS_OpenHighLow(),
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BreakSt}, SymbBreakSt_MakeFieldsTwf(),kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Closing}, SymbFuoClosing_MakeFields(),kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_DynBand}, SymbDynBand_MakeFields(),   kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_QuoteReq},SymbQuoteReq_MakeFields(),  kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Rt},      MdRtStream::MakeFields(),   kTabFlag}}
   )};
}
static const int32_t kExgMdSymbOffset[]{
   0, // Base
   fon9_OffsetOf(ExgMdSymb, Ref_),
   fon9_OffsetOf(ExgMdSymb, BS_),
   fon9_OffsetOf(ExgMdSymb, Deal_),
   f9fmkt_MAKE_OFFSET_OpenHighLow(ExgMdSymb),
   fon9_OffsetOf(ExgMdSymb, BreakSt_),
   fon9_OffsetOf(ExgMdSymb, FuoClosing_),
   fon9_OffsetOf(ExgMdSymb, DynBand_),
   fon9_OffsetOf(ExgMdSymb, QuoteReq_),
   fon9_OffsetOf(ExgMdSymb, MdRtStream_),
};
static inline fon9::fmkt::SymbData* GetExgMdSymbData(ExgMdSymb* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kExgMdSymbOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kExgMdSymbOffset[tabid])
      : nullptr;
}
fon9::fmkt::SymbData* ExgMdSymb::GetSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
fon9::fmkt::SymbData* ExgMdSymb::FetchSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
//--------------------------------------------------------------------------//
fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
ExgMdSymb::ExgMdSymb(const fon9::StrView& symbid, ExgMdSymbs& owner)
   : base{symbid}
   , MdRtStream_{owner.RtInnMgr_}
   , Contract_{owner.FetchContractAndAppendSymb(*this)} {
   this->TDayYYYYMMDD_ = owner.RtInnMgr_.TDayYYYYMMDD();
   this->MdRtStream_.OpenRtStorage(*this);
}
fon9_MSC_WARN_POP;

ExgMdSymb::~ExgMdSymb() {
}
void ExgMdSymb::OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(owner, *this);
   this->Contract_.OnSymbRemove(*this);
}
//----------------//
// 保留部分資料, SessionClear(); DailyClear(); 之後還原.
fon9_WARN_DISABLE_PADDING;
struct ExgMdSymb_ClearKeep {
   fon9_NON_COPY_NON_MOVE(ExgMdSymb_ClearKeep);
   ExgMdSymb&              Symb_;
   const uint32_t          PriceOrigDiv_, StrikePriceDiv_;
   ExgMdContract* const    Contract_;
   ExgMdSymb_ClearKeep(ExgMdSymb& symb)
      : Symb_(symb)
      , PriceOrigDiv_{symb.PriceOrigDiv_}
      , StrikePriceDiv_{symb.StrikePriceDiv_}
      // 新建商品後的清盤, 需根據契約還原基本資料.
      , Contract_(symb.TradingSessionId_ == f9fmkt_TradingSessionId_Unknown
                  ? &symb.Contract_ : nullptr) {
   }
   ~ExgMdSymb_ClearKeep() {
      this->Symb_.PriceOrigDiv_ = this->PriceOrigDiv_;
      this->Symb_.StrikePriceDiv_ = this->StrikePriceDiv_;
      if (this->Contract_) {
         // - 這裡處理的是: 後來加入 Contract 的商品.
         // - 若是在更早加入 Contract, 則會:
         //   在「收到 I010 觸發 ExgMdContract::OnSymbBaseChanged()」時,
         //   調整全部 Contract.Symbs 的 TradingSessionId;
         this->Symb_.FlowGroup_ = this->Contract_->FlowGroup_;
         for (ExgMdSymb* psymb : this->Contract_->Symbs_) {
            // 在 this->Contract_->Symbs_ 前面的 symb, 有可能透過非行情管道建立(例如: TwfC10Importer),
            // 造成 psymb->TradingSessionId_, psymb->TradingSessionSt_ 尚未設定正確.
            // 所以要排處這些 symb;
            if (psymb != &this->Symb_
             && psymb->TradingSessionId_ != f9fmkt_TradingSessionId_Unknown
             && psymb->TradingSessionSt_ != f9fmkt_TradingSessionSt_Unknown) {
               this->Symb_.TradingSessionId_ = psymb->TradingSessionId_;
               this->Symb_.TradingSessionSt_ = psymb->TradingSessionSt_;
               break;
            }
         }
      }
   }
};
fon9_WARN_POP;

void ExgMdSymb::SessionClear(fon9::fmkt::SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   ExgMdSymb_ClearKeep keeper{*this};
   base::SessionClear(owner, tsesId);
}
void ExgMdSymb::DailyClear(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) {
   ExgMdSymb_ClearKeep keeper{*this};
   base::DailyClear(owner, tdayYYYYMMDD);
}
void ExgMdSymb::OnOpenSeqReset(fon9::fmkt::SymbTree& owner) {
   fon9_WARN_DISABLE_SWITCH;
   switch (this->TradingSessionSt_) {
   case f9fmkt_TradingSessionSt_Open: // 只有在盤中的商品才需要 OpenSeqReset.
      this->TradingSessionSt_ = f9fmkt_TradingSessionSt_OpenSeqReset;
      this->MdRtStream_.OnOpenSeqReset(owner, *this);
      break;
   }
   fon9_WARN_POP;
}
//--------------------------------------------------------------------------//
ExgMdSymbs::ExgMdSymbs(std::string rtiPathFmt, bool isAddMarketSeq)
   : base(ExgMdSymb::MakeLayout(isAddMarketSeq), std::move(rtiPathFmt),
          isAddMarketSeq ? fon9::fmkt::MdSymbsCtrlFlag::HasMarketDataSeq : fon9::fmkt::MdSymbsCtrlFlag{}) {
}
fon9::fmkt::SymbSP ExgMdSymbs::MakeSymb(const fon9::StrView& symbid) {
   assert(this->SymbMap_.IsLocked());
   return new ExgMdSymb(symbid, *this);
}
void ExgMdSymbs::OnAfterLoadFrom(Locker&& symbsLk) {
   (void)symbsLk;
   this->Contracts_.OnSymbsReload();
}
void ExgMdSymbs::OnTreeOp(fon9::seed::FnTreeOp fnCallback) {
   MdTreeOp op{*this};
   fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
}
void ExgMdSymbs::MdTreeOp::Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) {
   Locker lockedMap{static_cast<SymbTreeT*>(&this->Tree_)->SymbMap_};
   auto   symb = static_cast<ExgMdSymbs*>(&this->Tree_)->GetSymb(lockedMap, strKeyText);
   // 如果 strKeyText 為正確的複式單 Id (例: "TXFB0/C0" or "MXFC6/MX4C6"),
   // 但商品不存在,
   // => 可能尚未收到行情資料.
   // => 此時應建立此複式商品.
   const char* const id1 = strKeyText.begin();
   const auto        idLen = strKeyText.size();
   if (fon9_UNLIKELY(!symb && id1[5] == '/' && (idLen == 8 || idLen == 11))) {
      // 2腳的商品都必須存在, 且 Leg1.Id < Leg2.Id (需注意跨年問題)
      char  id2buf[5];
      const char* id2;
      if (idLen == 11)
         id2 = id1 + 6;
      else {
         memcpy(id2buf, id1, 3);
         memcpy(id2buf + 3, id1 + 6, 2);
         id2 = id2buf;
      }
      auto leg1 = static_cast<ExgMdSymbs*>(&this->Tree_)->GetSymb(lockedMap, fon9::StrView{id1,5});
      auto leg2 = static_cast<ExgMdSymbs*>(&this->Tree_)->GetSymb(lockedMap, fon9::StrView{id2,5});
      if (leg1 && leg2
          && static_cast<ExgMdSymb*>(leg1.get())->EndYYYYMMDD_ < static_cast<ExgMdSymb*>(leg2.get())->EndYYYYMMDD_) {
         symb = static_cast<ExgMdSymbs*>(&this->Tree_)->FetchSymb(lockedMap, strKeyText);
         if (symb) {
            symb->TDayYYYYMMDD_ = leg1->TDayYYYYMMDD_;
            symb->TradingMarket_ = leg1->TradingMarket_;
            symb->TradingSessionId_ = leg1->TradingSessionId_;
            symb->TradingSessionSt_ = leg1->TradingSessionSt_;
            *static_cast<fon9::fmkt::SymbTwfBase*>(static_cast<ExgMdSymb*>(symb.get()))
               = *static_cast<fon9::fmkt::SymbTwfBase*>(static_cast<ExgMdSymb*>(leg1.get()));
         }
      }
   }
   this->OnPodOp(strKeyText, std::move(symb), std::move(fnCallback), lockedMap);
}
//--------------------------------------------------------------------------//
ExgMdSymbsMgr::ExgMdSymbsMgr(ExgMdSymbsSP symbs, fon9::StrView sysName, fon9::StrView groupName, f9fmkt_TradingSessionId tsesId)
   : TradingSessionId_{tsesId}
   , Name_{sysName.ToString() + "_" + groupName.ToString()}
   , Symbs_{std::move(symbs)} {
}
ExgMdSymbsMgr::~ExgMdSymbsMgr() {
}
//--------------------------------------------------------------------------//
f9twf_API const void* ExgMdToSnapshotBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry,
                                        fon9::fmkt::SymbTwfBS& symbBS, uint32_t priceOrigDiv, fon9::fmkt::MarketDataSeq mktseq) {
   symbBS.Data_.Clear(mdTime);
   symbBS.Data_.MarketSeq_ = mktseq;
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      switch (mdEntry->EntryType_) {
      case '0':
         if (lv >= symbBS.Data_.kBSCount)
            continue;
         dst = symbBS.Data_.Buys_ + lv;
         symbBS.Data_.Flags_ |= f9sv_BSFlag_OrderBuy;
         break;
      case '1':
         if (lv >= symbBS.Data_.kBSCount)
            continue;
         dst = symbBS.Data_.Sells_ + lv;
         symbBS.Data_.Flags_ |= f9sv_BSFlag_OrderSell;
         break;
      case 'E':
         dst = &symbBS.Data_.DerivedBuy_;
         symbBS.Data_.Flags_ |= f9sv_BSFlag_DerivedBuy;
         break;
      case 'F':
         dst = &symbBS.Data_.DerivedSell_;
         symbBS.Data_.Flags_ |= f9sv_BSFlag_DerivedSell;
         break;
      default:
         continue;
      }
      mdEntry->Price_.AssignTo(dst->Pri_, priceOrigDiv);
      dst->Qty_ = fon9::PackBcdTo<uint32_t>(mdEntry->Qty_);
   }
   return mdEntry;
}
f9twf_API void ExgMdToUpdateBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry,
                               fon9::fmkt::SymbTwfBS& symbBS, uint32_t priceOrigDiv, fon9::fmkt::MarketDataSeq mktseq) {
   symbBS.Data_.InfoTime_ = mdTime;
   symbBS.Data_.Flags_ = f9sv_BSFlag{};
   symbBS.Data_.MarketSeq_ = mktseq;
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      unsigned dstCount;
      switch (mdEntry->EntryType_) {
      case '0':
         symbBS.Data_.Flags_ |= f9sv_BSFlag_OrderBuy;
         dst = symbBS.Data_.Buys_;
         dstCount = symbBS.Data_.kBSCount;
         break;
      case '1':
         symbBS.Data_.Flags_ |= f9sv_BSFlag_OrderSell;
         dst = symbBS.Data_.Sells_;
         dstCount = symbBS.Data_.kBSCount;
         break;
      case 'E':
         symbBS.Data_.Flags_ |= f9sv_BSFlag_DerivedBuy;
         dst = &symbBS.Data_.DerivedBuy_;
         dstCount = 1;
         break;
      case 'F':
         symbBS.Data_.Flags_ |= f9sv_BSFlag_DerivedSell;
         dst = &symbBS.Data_.DerivedSell_;
         dstCount = 1;
         break;
      default:
         continue;
      }
      if (lv >= dstCount)
         continue;
      dst += lv;
      switch (mdEntry->UpdateAction_) {
      case '2': // Delete.
         if (auto mvc = dstCount - lv - 1)
            memmove(dst, dst + 1, mvc * sizeof(*dst));
         fon9::ForceZeroNonTrivial(dst + dstCount - lv - 1);
         break;
      case '0': // New.
         if (auto mvc = dstCount - lv - 1)
            memmove(dst + 1, dst, mvc * sizeof(*dst));
         /* fall through */ // 繼續設定新的價量.
      case '1': // Change.
      case '5': // Overlay.
         mdEntry->Price_.AssignTo(dst->Pri_, priceOrigDiv);
         dst->Qty_ = fon9::PackBcdTo<uint32_t>(mdEntry->Qty_);
         break;
      default:
         continue;
      }
   }
}

} // namespaces
