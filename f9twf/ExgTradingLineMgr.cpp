// \file f9twf/ExgTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgTradingLineMgr.hpp"

namespace f9twf {

void ExgIoManager::OnParentSeedClear() {
   this->RegularLineMgr_.OnBeforeDestroy();
   this->AfterHourLineMgr_.OnBeforeDestroy();
   base::OnParentSeedClear();
}

} // namespaces
