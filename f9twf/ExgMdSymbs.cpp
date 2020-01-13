// \file f9twf/ExgMdSymbs.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdSymbs.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9twf {

static const int32_t kExgMdSymbOffset[]{
   0,
   fon9_OffsetOf(ExgMdSymb, Ref_),
   fon9_OffsetOf(ExgMdSymb, BS_),
   fon9_OffsetOf(ExgMdSymb, Deal_),
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
fon9::seed::LayoutSP ExgMdSymb::MakeLayout() {
   using namespace fon9::seed;
   using namespace fon9::fmkt;
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{fon9::Named{"Base"}, Symb::MakeFields(),     TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{"Ref"},  SymbRef::MakeFields(),  TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{"BS"},   SymbBS::MakeFields(),   TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{"Deal"}, SymbDeal::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}}
   )};
}
void ExgMdSymb::DailyClear() {
   this->BasicInfoTime_.AssignNull(); if (0);// 或是應該用 I011.END-SESSION 判斷 '0'一般交易時段, '1'盤後交易時段.
   this->FlowGroup_ = 0;
   this->ExgSymbSeq_ = 0;
   this->Ref_.DailyClear();
   this->Deal_.DailyClear();
   this->BS_.DailyClear();
}
//--------------------------------------------------------------------------//
ExgMdSymbs::ExgMdSymbs() : base{ExgMdSymb::MakeLayout()} {
}
void ExgMdSymbs::DailyClear() {
   auto symbs = this->SymbMap_.Lock();
   for (auto& symb : *symbs)
      static_cast<ExgMdSymb*>(symb.second.get())->DailyClear();
}
fon9::fmkt::SymbSP ExgMdSymbs::MakeSymb(const fon9::StrView& symbid) {
   return fon9::fmkt::SymbSP{new ExgMdSymb(symbid)};
}
//--------------------------------------------------------------------------//
f9twf_API const void* ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry,
                                         fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv) {
   symbBS.Clear(mdTime);
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      switch (mdEntry->EntryType_) {
      case '0':
         if (lv >= symbBS.kBSCount)
            continue;
         dst = symbBS.Data_.Buys_ + lv;
         break;
      case '1':
         if (lv >= symbBS.kBSCount)
            continue;
         dst = symbBS.Data_.Sells_ + lv;
         break;
      case 'E':
         dst = &symbBS.Data_.DerivedBuy_;
         break;
      case 'F':
         dst = &symbBS.Data_.DerivedSell_;
         break;
      default:
         continue;
      }
      mdEntry->Price_.AssignTo(dst->Pri_, priceOrigDiv);
      dst->Qty_ = fon9::PackBcdTo<uint32_t>(mdEntry->Qty_);
   }
   return mdEntry;
}
f9twf_API void ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry,
                                  fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv) {
   symbBS.Data_.Time_ = mdTime;
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      unsigned dstCount;
      switch (mdEntry->EntryType_) {
      case '0':
         dst = symbBS.Data_.Buys_;
         dstCount = symbBS.kBSCount;
         break;
      case '1':
         dst = symbBS.Data_.Sells_;
         dstCount = symbBS.kBSCount;
         break;
      case 'E':
         dst = &symbBS.Data_.DerivedBuy_;
         dstCount = 1;
         break;
      case 'F':
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
