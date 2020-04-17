// \file fon9/fmkt/SymbTwf.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTwf.hpp"

namespace fon9 { namespace fmkt {

SymbTwf::~SymbTwf() {
}
bool SymbTwf::IsExpired(unsigned tdayYYYYMMDD) const {
   return IsSymbExpired(this->EndYYYYMMDD_, tdayYYYYMMDD);
}
void SymbTwf::OnSymbDailyClear(SymbTree& tree, const Symb& symb) {
   base::OnSymbDailyClear(tree, symb);
   this->SymbTwfBaseDailyClear();
}
void SymbTwf::OnSymbSessionClear(SymbTree& tree, const Symb& symb) {
   base::OnSymbSessionClear(tree, symb);
   this->SymbTwfBaseSessionClear();
}
seed::Fields SymbTwf::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwfBase::AddFields<SymbTwf>(flds);
   return flds;
}

} } // namespaces
