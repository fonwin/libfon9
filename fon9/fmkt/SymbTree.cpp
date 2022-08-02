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
//--------------------------------------------------------------------------//
MdSymbTree::~MdSymbTree() {
}
void MdSymbTree::LockedDailyClear(Locker& symbs, unsigned tdayYYYYMMDD) {
   auto iend = symbs->end();
   for (auto ibeg = symbs->begin(); ibeg != iend;) {
      auto& symb = *ibeg->second;
      if (!symb.IsExpired(tdayYYYYMMDD)) {
         symb.DailyClear(*this, tdayYYYYMMDD);
         ++ibeg;
      }
      else { // 移除已下市商品, 移除時需觸發 PodRemoved 事件.
         symb.OnBeforeRemove(*this, tdayYYYYMMDD);
         ibeg = symbs->erase(ibeg);
      }
   }
}

} } // namespaces
