// \file f9twf/ExgTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgTradingLineMgr.hpp"

namespace f9twf {

void ExgTradingLineMgr::OnParentSeedClear() {
   this->OnBeforeDestroy();
   baseIo::OnParentSeedClear();
}
void ExgTradingLineMgr::OnTDayChanged(fon9::TimeStamp tday, std::string cause) {
   (void)tday;
   this->DisposeAndReopen(std::move(cause));
}

} // namespaces
