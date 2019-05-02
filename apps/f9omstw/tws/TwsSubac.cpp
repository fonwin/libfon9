// \file tws/TwsSubac.cpp
// \author fonwinz@gmail.com
#include "tws/TwsSubac.hpp"
#include "tws/TwsIvSymb.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

fon9::seed::LayoutSP TwsSubac::MakeLayout() {
   using namespace fon9::seed;
   Fields infoflds;
   infoflds.Add(fon9_MakeField(fon9::Named{"OrderLimit"}, TwsSubacInfo, OrderLimit_));
   return new LayoutN(fon9_MakeField(fon9::Named{"SubacNo"}, TwsSubac, SubacNo_), TreeFlag::AddableRemovable,
      new Tab{fon9::Named{"Info"}, std::move(infoflds), TabFlag::NoSeedCommand | TabFlag::Writable},
      new Tab{fon9::Named{"Sc"}, TwsIvSc::MakeFields(), TwsIvSymb::MakeLayout(), TabFlag::NoSeedCommand | TabFlag::Writable | TabFlag::HasSapling}
   );
   #define kTabIndex_Info     0
   #define kTabIndex_Sc       1
}
template <class RawBase>
struct SubacSeedRaw : public RawBase {
   fon9_NON_COPY_NON_MOVE(SubacSeedRaw);
   SubacSeedRaw(int tabIndex, TwsSubac& subac)
      : RawBase{tabIndex == kTabIndex_Sc ? fon9::seed::CastToRawPointer(&subac.Sc_) : fon9::seed::CastToRawPointer(&subac.Info_)} {
      assert(tabIndex == kTabIndex_Sc || tabIndex == kTabIndex_Info);
   }
};
using SubacSeedRd = SubacSeedRaw<fon9::seed::SimpleRawRd>;
using SubacSeedWr = SubacSeedRaw<fon9::seed::SimpleRawWr>;

void TwsSubac::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, SubacSeedRd{tab->GetIndex(), *this}, rbuf, fon9::seed::GridViewResult::kCellSplitter);
   fon9::RevPrint(rbuf, this->SubacNo_);
}

struct TwsSubac::PodOp : public fon9::seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = fon9::seed::PodOpDefault;
public:
   TwsSubac* Subac_;
   PodOp(OmsTree& sender, TwsSubac* subac, const fon9::StrView& strKeyText)
      : base(sender, fon9::seed::OpResult::no_error, strKeyText)
      , Subac_{subac} {
   }
   void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override {
      assert(OmsSubacTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), SubacSeedRd{tab.GetIndex(), *this->Subac_});
   }
   void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override {
      assert(OmsSubacTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), SubacSeedWr{tab.GetIndex(), *this->Subac_});
   }
   fon9::seed::TreeSP GetSapling(fon9::seed::Tab& tab) override {
      assert(OmsSubacTree::IsInOmsThread(this->Sender_));
      switch (tab.GetIndex()) {
      case kTabIndex_Sc:
         return new OmsIvSymbTree(static_cast<OmsSubacTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_,
                                  this->Subac_, this->Subac_->Sc_.Symbs_, &TwsIvSymb::SymbMaker);
      }
      return nullptr;
   }
};
void TwsSubac::OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) {
   PodOp op{ownerTree, this, strKeyText};
   fnCallback(op, &op);
}

} // namespaces
