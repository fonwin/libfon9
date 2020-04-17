// \file f9twf/ExgMdContracts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdContracts.hpp"
#include "f9twf/ExgMdSymbs.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9twf {
using namespace fon9::seed;
using namespace fon9::fmkt;

void ExgMdContract::OnSymbRemove(ExgMdSymb& symb) {
   auto ifind = std::find(this->Symbs_.begin(), this->Symbs_.end(), &symb);
   if (ifind != this->Symbs_.end())
      this->Symbs_.erase(ifind);
}

void ExgMdContract::AssignBaseToSymb(ExgMdSymb& symb) const {
   symb.TradingMarket_ = this->TradingMarket_;
   symb.FlowGroup_ = this->FlowGroup_;
   symb.StrikePriceDiv_ = this->StrikePriceDiv_;
   symb.PriceOrigDiv_ = this->PriceOrigDiv_;
}
inline bool IsContractBaseChanged(const ExgMdContract& con, const ExgMdSymb& symb) {
   return con.TradingMarket_ != symb.TradingMarket_
      || con.FlowGroup_ != symb.FlowGroup_
      || con.PriceOrigDiv_ != symb.PriceOrigDiv_
      || con.StrikePriceDiv_ != symb.StrikePriceDiv_;
}
void ExgMdContract::OnSymbBaseChanged(const ExgMdSymb& src) {
   this->SetBaseValues(src.TradingMarket_, src.FlowGroup_, src.PriceOrigDiv_, src.StrikePriceDiv_);
}
void ExgMdContract::SetBaseValues(f9fmkt_TradingMarket mkt,
                                  SymbFlowGroup_t flowGroup,
                                  uint32_t priceOrigDiv,
                                  uint32_t strikePriceDiv) {
   if (this->TradingMarket_ == mkt
       && this->FlowGroup_ == flowGroup
       && this->PriceOrigDiv_ == priceOrigDiv
       && this->StrikePriceDiv_ == strikePriceDiv)
      return;
   this->TradingMarket_ = mkt;
   this->FlowGroup_ = flowGroup;
   this->PriceOrigDiv_ = priceOrigDiv;
   this->StrikePriceDiv_ = strikePriceDiv;
   for (ExgMdSymb* psymb : this->Symbs_) {
      if (IsContractBaseChanged(*this, *psymb)) {
         this->AssignBaseToSymb(*psymb);
         // TODO: 通知 this->Symbs_ 的 Contract 基本資料有異動?
         // - 會來到這裡的情況, 只有: (1)「期貨價差」商品; (2)遺漏I010基本資料的商品;
         // - 目前只有 StrikePriceDiv_, PriceOrigDiv_, TradingMarket_, FlowGroup_;
         // - 所以還不需要額外產生事件通知.
      }
   }
}
void ExgMdContract::OnSymbsReload() {
   for (ExgMdSymb* psymb : this->Symbs_) {
      if (IsContractBaseChanged(*this, *psymb)) {
         if (psymb->PriceOrigDiv_ == 0 || psymb->StrikePriceDiv_ == 0) {
            this->AssignBaseToSymb(*psymb);
            continue;
         }
         this->OnSymbBaseChanged(*psymb);
         break;
      }
   }
}
//--------------------------------------------------------------------------//
ExgMdContracts::ExgMdContracts() {
}
void ExgMdContracts::OnSymbsReload() {
   for (auto& con : this->ContractMap_)
      con->OnSymbsReload();
}
ExgMdContract& ExgMdContracts::FetchContract(ContractId conId) {
   auto ifind = this->ContractMap_.find(conId);
   if (ifind == this->ContractMap_.end())
      ifind = this->ContractMap_.insert(ContractSP{new ExgMdContract{conId}}).first;
   return *ifind->get();
}
ExgMdContract& ExgMdContracts::FetchContract(ExgMdSymb& symb) {
   ExgMdContract& retval = this->FetchContract(ToStrView(symb.SymbId_));
   retval.Symbs_.push_back(&symb);
   retval.AssignBaseToSymb(symb);
   return retval;
}

} // namespaces
