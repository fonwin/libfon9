// \file tws/TwsBrk.cpp
// \author fonwinz@gmail.com
#include "tws/TwsBrk.hpp"
#include "tws/TwsIvac.hpp"
#include "f9omstw/OmsBrkTree.hpp"
#include "f9omstw/OmsIvacTree.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

OmsIvacSP TwsBrk::MakeIvac(IvacNo ivacNo) {
   return new TwsIvac(ivacNo, this);
}
//--------------------------------------------------------------------------//
fon9::seed::LayoutSP TwsBrk::MakeLayout(fon9::seed::TreeFlag treeflags) {
   using namespace fon9::seed;
   return new Layout1(fon9_MakeField(fon9::Named{"BrkId"}, TwsBrk, BrkId_), treeflags,
               new Tab{fon9::Named{"Base"}, Fields{},
                      TwsIvac::MakeLayout(OmsIvacTree::DefaultTreeFlag()),
                      TabFlag::NoSeedCommand}
               );
}
void TwsBrk::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, fon9::seed::SimpleRawRd{*this}, rbuf, fon9::seed::GridViewResult::kCellSplitter);
   fon9::RevPrint(rbuf, this->BrkId_);
}
//--------------------------------------------------------------------------//
struct TwsBrk::PodOp : public fon9::seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = fon9::seed::PodOpDefault;
public:
   TwsBrk* Brk_;
   PodOp(OmsBrkTree& sender, TwsBrk* brk)
      : base(sender, fon9::seed::OpResult::no_error, ToStrView(brk->BrkId_))
      , Brk_{brk} {
   }
   // 除了 key=BrkId 沒有其他欄位, 所以沒有 BeginRead(); BeginWrite(); 的需要.
   // void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override;
   // void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override;

   fon9::seed::TreeSP GetSapling(fon9::seed::Tab& tab) override {
      assert(OmsBrkTree::IsInOmsThread(this->Sender_));
      return new OmsIvacTree(static_cast<OmsBrkTree*>(this->Sender_)->OmsCore_, tab.SaplingLayout_, this->Brk_);
   }
};
void TwsBrk::OnPodOp(OmsBrkTree& brkTree, fon9::seed::FnPodOp&& fnCallback) {
   PodOp op{brkTree, this};
   fnCallback(op, &op);
}

} // namespaces
