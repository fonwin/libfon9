// \file tws/TwsIvSymb.cpp
// \author fonwinz@gmail.com
#include "tws/TwsIvSymb.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

static fon9::seed::Fields TwsIvSymb_MakeFields() {
   fon9::seed::Fields   flds;
   flds.Add(fon9_MakeField(fon9::Named{"GnQty"}, TwsIvSymbSc, GnQty_));
   flds.Add(fon9_MakeField(fon9::Named{"CrQty"}, TwsIvSymbSc, CrQty_));
   flds.Add(fon9_MakeField(fon9::Named{"CrAmt"}, TwsIvSymbSc, CrAmt_));
   flds.Add(fon9_MakeField(fon9::Named{"DbQty"}, TwsIvSymbSc, DbQty_));
   flds.Add(fon9_MakeField(fon9::Named{"DbAmt"}, TwsIvSymbSc, DbAmt_));
   return flds;
}
fon9::seed::LayoutSP TwsIvSymb::MakeLayout() {
   using namespace fon9::seed;
   return new LayoutN(fon9_MakeField(fon9::Named{"SymbId"}, TwsIvSymb, SymbId_), TreeFlag::AddableRemovable,
      new Tab{fon9::Named{"Bal"}, TwsIvSymb_MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable},
      new Tab{fon9::Named{"Ord"}, TwsIvSymb_MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable},
      new Tab{fon9::Named{"Mat"}, TwsIvSymb_MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}
   );
   #define kTabIndex_Bal   0
   #define kTabIndex_Ord   1
   #define kTabIndex_Mat   2
}

template <class RawBase>
struct TwsIvSymbRaw : public RawBase {
   fon9_NON_COPY_NON_MOVE(TwsIvSymbRaw);
   static fon9::byte* CastToRawPointer(int tabIndex, TwsIvSymb& symb) {
      switch (tabIndex) {
      case kTabIndex_Bal:  return fon9::seed::CastToRawPointer(&symb.Bal_);
      case kTabIndex_Ord:  return fon9::seed::CastToRawPointer(&symb.Ord_);
      case kTabIndex_Mat:  return fon9::seed::CastToRawPointer(&symb.Mat_);
      }
      assert(!"UomsIvSymbRaw|err=Unknown tabIndex");
      return nullptr;
   }
   TwsIvSymbRaw(int tabIndex, TwsIvSymb& symb)
      : RawBase{CastToRawPointer(tabIndex, symb)} {
   }
};
using TwsIvSymbRd = TwsIvSymbRaw<fon9::seed::SimpleRawRd>;
using TwsIvSymbWr = TwsIvSymbRaw<fon9::seed::SimpleRawWr>;

void TwsIvSymb::MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, TwsIvSymbRd{tab->GetIndex(), *this}, rbuf, fon9::seed::GridViewResult::kCellSplitter);
   fon9::RevPrint(rbuf, this->SymbId_);
}

struct TwsIvSymb::PodOp : public fon9::seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = fon9::seed::PodOpDefault;
public:
   TwsIvSymb* Symb_;
   PodOp(fon9::seed::Tree& sender, TwsIvSymb* symb, const fon9::StrView& strKeyText)
      : base(sender, fon9::seed::OpResult::no_error, strKeyText)
      , Symb_{symb} {
   }
   void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override {
      assert(OmsIvSymbTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), TwsIvSymbRd{tab.GetIndex(), *this->Symb_});
   }
   void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override {
      assert(OmsIvSymbTree::IsInOmsThread(this->Sender_));
      this->BeginRW(tab, std::move(fnCallback), TwsIvSymbWr{tab.GetIndex(), *this->Symb_});
   }
};
void TwsIvSymb::OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) {
   PodOp op{ownerTree, this, strKeyText};
   fnCallback(op, &op);
}

} // namespaces
