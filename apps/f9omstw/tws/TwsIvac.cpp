// \file tws/TwsIvac.cpp
// \author fonwinz@gmail.com
#include "tws/TwsIvac.hpp"
#include "tws/TwsSubac.hpp"
#include "tws/TwsIvSymb.hpp"
#include "f9omstw/OmsIvacTree.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

TwsSubacSP TwsIvac::RemoveSubac(fon9::StrView subacNo) {
   return fon9::static_pointer_cast<TwsSubac>(ContainerRemove(this->Subacs_, subacNo));
}
TwsSubac* TwsIvac::GetSubac(fon9::StrView subacNo) const {
   auto ifind = this->Subacs_.find(subacNo);
   return (ifind == this->Subacs_.end() ? nullptr : static_cast<TwsSubac*>(ifind->get()));
}
TwsSubac* TwsIvac::FetchSubac(fon9::StrView subacNo) {
   auto ifind = this->Subacs_.find(subacNo);
   if (ifind == this->Subacs_.end())
      ifind = this->Subacs_.insert(new TwsSubac(subacNo, this)).first;
   return static_cast<TwsSubac*>(ifind->get());
}
//--------------------------------------------------------------------------//
fon9::seed::LayoutSP TwsIvac::MakeLayout(fon9::seed::TreeFlag treeflags) {
   using namespace fon9::seed;
   Fields   infoflds;
   infoflds.Add(fon9_MakeField(fon9::Named{"Name"},       TwsIvacInfo, Name_));
   infoflds.Add(fon9_MakeField(fon9::Named{"OrderLimit"}, TwsIvacInfo, OrderLimit_));
   return new LayoutN(fon9_MakeField(fon9::Named{"IvacNo"}, TwsIvac, IvacNo_), treeflags,
      new Tab{fon9::Named{"Info"}, std::move(infoflds), TwsSubac::MakeLayout(), TabFlag::NoSeedCommand | TabFlag::Writable | TabFlag::HasSapling},
      new Tab{fon9::Named{"Sc"}, TwsIvSc::MakeFields(), TwsIvSymb::MakeLayout(), TabFlag::NoSeedCommand | TabFlag::Writable | TabFlag::HasSapling}
   );
   #define kTabIndex_Info     0
   #define kTabIndex_Sc       1
}
template <class RawBase>
struct IvacSeedRaw : public RawBase {
   fon9_NON_COPY_NON_MOVE(IvacSeedRaw);
   IvacSeedRaw(int tabIndex, TwsIvac& ivac)
      : RawBase{tabIndex == kTabIndex_Sc ? fon9::seed::CastToRawPointer(&ivac.Sc_) : fon9::seed::CastToRawPointer(&ivac.Info_)} {
      assert(tabIndex == kTabIndex_Sc || tabIndex == kTabIndex_Info);
   }
};
using IvacSeedRd = IvacSeedRaw<fon9::seed::SimpleRawRd>;
using IvacSeedWr = IvacSeedRaw<fon9::seed::SimpleRawWr>;

void TwsIvac::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, IvacSeedRd{tab->GetIndex(), *this}, rbuf, fon9::seed::GridViewResult::kCellSplitter);
   fon9::RevPrint(rbuf, this->IvacNo_);
}

struct TwsIvac::PodOp : public fon9::seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = fon9::seed::PodOpDefault;
public:
   TwsIvac* Ivac_;
   PodOp(OmsIvacTree& sender, TwsIvac* ivac, const fon9::StrView& strKeyText)
      : base(sender, fon9::seed::OpResult::no_error, strKeyText)
      , Ivac_{ivac} {
   }
   void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override {
      assert(OmsIvacTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), IvacSeedRd{tab.GetIndex(), *this->Ivac_});
   }
   void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override {
      assert(OmsIvacTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), IvacSeedWr{tab.GetIndex(), *this->Ivac_});
   }

   static OmsSubacSP SubacMaker(const fon9::StrView& subacNo, OmsIvBase* owner) {
      assert(dynamic_cast<TwsIvac*>(owner) != nullptr);
      return new TwsSubac(subacNo, static_cast<TwsIvac*>(owner));
   }
   fon9::seed::TreeSP GetSapling(fon9::seed::Tab& tab) override {
      assert(OmsIvacTree::IsInOmsThread(this->Sender_));
      switch (tab.GetIndex()) {
      case kTabIndex_Info:
         return new OmsSubacTree(static_cast<OmsIvacTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_,
                                 this->Ivac_, this->Ivac_->Subacs_, &SubacMaker);
      case kTabIndex_Sc:
         return new OmsIvSymbTree(static_cast<OmsIvacTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_,
                                  this->Ivac_, this->Ivac_->Sc_.Symbs_, &TwsIvSymb::SymbMaker);
      }
      return nullptr;
   }
};
void TwsIvac::OnPodOp(OmsIvacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) {
   PodOp op{ownerTree, this, strKeyText};
   fnCallback(op, &op);
}
void TwsIvac::OnParentSeedClear() {
   this->Subacs_.clear();
}

} // namespaces
