// \file f9twf/ExgMdSymbs.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdSymbs.hpp"
#include "f9twf/ExgMdContracts.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9twf {

fon9::seed::LayoutSP ExgMdSymb::MakeLayout() {
   using namespace fon9;
   using namespace fon9::seed;
   using namespace fon9::fmkt;
   // -----
   // TODO:
   // -----
   // Add [Tab:詢價]:
   //    詢價揭示時間(DISCLOSURE TIME), 詢價持續時間(999 秒: DURATION TIME);
   // -----
   // Add [Tab:收盤後資料]:
   //    結算價(SETTLEMENT PRICE);
   //    未平倉合約數(OPEN INTEREST);
   //    鉅額交易總成交量(BLOCK TRADE QTY);
   // -----
   // 新增解析的格式, 要在建立 ExgMcChannelMgr 之後, 透過 ExgMcChannelMgr::RegMcMessageParser() 註冊;
   // -----
   constexpr auto kTabFlag = TabFlag::NoSapling_NoSeedCommand_Writable;
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Base}, MakeFields(),            kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Ref},  TwfSymbRef_MakeFields(), kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BS},   SymbBS_MakeFields(),     kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Deal}, SymbDeal_MakeFields(),   kTabFlag}},
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
   , Contract_{owner.FetchContract(*this)} {
   this->TDayYYYYMMDD_ = owner.RtInnMgr_.TDayYYYYMMDD();
}
fon9_MSC_WARN_POP;

ExgMdSymb::~ExgMdSymb() {
}
void ExgMdSymb::OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(owner, *this);
   this->Contract_.OnSymbRemove(*this);
}
//--------------------------------------------------------------------------//
ExgMdSymbs::ExgMdSymbs(std::string rtiPathFmt)
   : base(ExgMdSymb::MakeLayout(), std::move(rtiPathFmt)) {
}
fon9::fmkt::SymbSP ExgMdSymbs::MakeSymb(const fon9::StrView& symbid) {
   assert(this->SymbMap_.IsLocked());
   return new ExgMdSymb(symbid, *this);
}
void ExgMdSymbs::OnAfterLoadFrom(Locker&& symbsLk) {
   (void)symbsLk;
   this->Contracts_.OnSymbsReload();
}
//--------------------------------------------------------------------------//
f9twf_API const void* ExgMdToSnapshotBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry,
                                        fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv) {
   symbBS.Data_.Clear(mdTime);
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      switch (mdEntry->EntryType_) {
      case '0':
         if (lv >= symbBS.Data_.kBSCount)
            continue;
         dst = symbBS.Data_.Buys_ + lv;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderBuy;
         break;
      case '1':
         if (lv >= symbBS.Data_.kBSCount)
            continue;
         dst = symbBS.Data_.Sells_ + lv;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderSell;
         break;
      case 'E':
         dst = &symbBS.Data_.DerivedBuy_;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedBuy;
         break;
      case 'F':
         dst = &symbBS.Data_.DerivedSell_;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedSell;
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
                               fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv) {
   symbBS.Data_.InfoTime_ = mdTime;
   symbBS.Data_.Flags_ = fon9::fmkt::BSFlag{};
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      unsigned dstCount;
      switch (mdEntry->EntryType_) {
      case '0':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderBuy;
         dst = symbBS.Data_.Buys_;
         dstCount = symbBS.Data_.kBSCount;
         break;
      case '1':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderSell;
         dst = symbBS.Data_.Sells_;
         dstCount = symbBS.Data_.kBSCount;
         break;
      case 'E':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedBuy;
         dst = &symbBS.Data_.DerivedBuy_;
         dstCount = 1;
         break;
      case 'F':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedSell;
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
         memset(dst + dstCount - lv - 1, 0, sizeof(*dst));
         break;
      case '0': // New.
         if (auto mvc = dstCount - lv - 1)
            memmove(dst + 1, dst, mvc * sizeof(*dst));
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
