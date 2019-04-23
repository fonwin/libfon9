// \file fon9/fmkt/SymbDy.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbDy.hpp"

namespace fon9 { namespace fmkt {

SymbDy::SymbDy(SymbDyTreeSP symbTreeSP, const StrView& symbid) : base{symbid}, SymbTreeSP_{std::move(symbTreeSP)} {
}
SymbData* SymbDy::GetSymbData(int tabid) {
   if (tabid <= 0)
      return this;
   ExDatas::Locker lk{this->ExDatas_};
   return (static_cast<size_t>(tabid) <= lk->size() ? (*lk)[static_cast<size_t>(tabid - 1)].get() : nullptr);
}
SymbData* SymbDy::FetchSymbData(int tabid) {
   if (tabid <= 0)
      return this;
   SymbDataTab* tab = dynamic_cast<SymbDataTab*>(this->SymbTreeSP_->LayoutSP_->GetTab(static_cast<size_t>(tabid)));
   if (tab == nullptr)
      return nullptr;
   ExDatas::Locker lk{this->ExDatas_};
   if (lk->size() < static_cast<size_t>(tabid))
      lk->resize(static_cast<size_t>(tabid));
   if (SymbData* dat = (*lk)[static_cast<size_t>(tabid - 1)].get())
      return dat;
   return ((*lk)[static_cast<size_t>(tabid - 1)] = tab->FetchSymbData(*this)).get();
}

//--------------------------------------------------------------------------//

SymbSP SymbDyTree::MakeSymb(const StrView& symbid) {
   return SymbSP{new SymbDy(this, symbid)};
}

} } // namespaces
