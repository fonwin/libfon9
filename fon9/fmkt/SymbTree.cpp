// \file fon9/fmkt/SymbTree.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace fmkt {

void SymbPodOp::OnAfterPodOpWrite(seed::Tab& tab) {
   (void)tab;
}
void SymbPodOp::BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) {
   if (auto dat = this->Symb_->GetSymbData(tab.GetIndex()))
      this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawRd{*dat});
   else {
      this->OpResult_ = seed::OpResult::not_found_seed;
      fnCallback(*this, nullptr);
   }
}
void SymbPodOp::BeginWrite(seed::Tab& tab, seed::FnWriteOp fnCallback) {
   if (auto dat = this->Symb_->FetchSymbData(tab.GetIndex())) {
      this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawWr{*dat});
      this->OnAfterPodOpWrite(tab);
   }
   else {
      this->OpResult_ = seed::OpResult::not_found_seed;
      fnCallback(*this, nullptr);
   }
}
//--------------------------------------------------------------------------//
SymbTree::~SymbTree() {
}

} } // namespaces
