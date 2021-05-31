// \file f9twf/ExgMdContracts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdContracts.hpp"
#include "f9twf/ExgMdSymbs.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9twf {
using namespace fon9::seed;
using namespace fon9::fmkt;

void ExgMdContract::SetContractBaseValues(f9fmkt_TradingMarket mkt,
                                          uint32_t priceOrigDiv,
                                          uint32_t strikePriceDiv) {
   if (this->TradingMarket_ == mkt
       && this->PriceOrigDiv_ == priceOrigDiv
       && this->StrikePriceDiv_ == strikePriceDiv)
      return;
   this->TradingMarket_ = mkt;
   this->PriceOrigDiv_ = priceOrigDiv;
   this->StrikePriceDiv_ = strikePriceDiv;
   for (ExgMdSymb* psymb : this->Symbs_) {
      // TODO: 通知 psymb: Contract 基本資料有異動?
      // - 會來到這裡的情況, 只有: (1)「期貨價差」商品; (2)遺漏(尚未收到)I010基本資料的商品;
      // - 目前只有 StrikePriceDiv_, PriceOrigDiv_, TradingMarket_;
      // - 所以還不需要額外產生事件通知.
      psymb->TradingMarket_  = mkt;
      psymb->PriceOrigDiv_   = priceOrigDiv;
      psymb->StrikePriceDiv_ = strikePriceDiv;
   }
}
void ExgMdContract::OnSymbRemove(ExgMdSymb& symb) {
   auto ifind = std::find(this->Symbs_.begin(), this->Symbs_.end(), &symb);
   if (ifind != this->Symbs_.end())
      this->Symbs_.erase(ifind);
}
// --------------------------------------------------- //
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
void ExgMdContract::OnSymbBaseChanged(const ExgMdSymb& src, ExgMdSymbsMgr* symbsMgr) {
   if (this->TradingMarket_ == src.TradingMarket_
       && this->FlowGroup_ == src.FlowGroup_
       && this->PriceOrigDiv_ == src.PriceOrigDiv_
       && this->StrikePriceDiv_ == src.StrikePriceDiv_)
      return;
   this->TradingMarket_ = src.TradingMarket_;
   this->FlowGroup_ = src.FlowGroup_;
   this->PriceOrigDiv_ = src.PriceOrigDiv_;
   this->StrikePriceDiv_ = src.StrikePriceDiv_;
   fon9::RevBufferFixedSize<128> cRts;
   fon9::StrView                 cStr;
   for (ExgMdSymb* psymb : this->Symbs_) {
      if (symbsMgr && !symbsMgr->CheckSymbTradingSessionId(*psymb))
         continue;
      if (!IsContractBaseChanged(*this, *psymb))
         continue;
      const auto oldFlowGroup = psymb->FlowGroup_;
      this->AssignBaseToSymb(*psymb);
      if (oldFlowGroup == psymb->FlowGroup_ || symbsMgr == nullptr)
         continue;
      // TODO: 通知 this->Symbs_ 的 Contract 基本資料有異動?
      // - 會來到這裡的情況, 只有: (1)「期貨價差」商品; (2)遺漏(尚未收到)I010基本資料的商品;
      // - 目前只有 StrikePriceDiv_, PriceOrigDiv_, TradingMarket_, FlowGroup_;
      // - 夜盤: 期貨價差商品, FlowGroup 會變動,
      //   但是期交所沒有提供[期貨價差商品]基本資料訊息, 所以只有在這裡才有機會通知.
      //   因此需要針對[期貨價差商品]觸發 FlowGroup 異動事件.
      switch (psymb->SymbId_.size()) {
      case 8:  // "TXFF3/I3"
      case 11: // "MXFC6/MX4C6"
         if (psymb->SymbId_.begin()[5] != '/')
            continue;
         if (cStr.empty()) {
            auto& flds = symbsMgr->Symbs_->LayoutSP_->GetTab(fon9_kCSTR_TabName_Base)->Fields_;
            // SessionId 變動(SessionSt=10:Clear) => FlowGroup 變動
            // 所以要發行 FlowGroup, Session, SessionSt;
            // -----
            fon9::ToBitv(cRts, psymb->FlowGroup_);
            fon9::fmkt::MdRtsPackFieldValueNid(cRts, *flds.Get("FlowGroup"));
            // -----
            fon9::ToBitv(cRts, psymb->TradingSessionId_);
            fon9::fmkt::MdRtsPackFieldValueNid(cRts, *flds.Get("Session"));
            // -----
            fon9::ToBitv(cRts, psymb->TradingSessionSt_);
            fon9::fmkt::MdRtsPackFieldValueNid(cRts, *flds.Get("SessionSt"));
            // -----
            cStr = ToStrView(cRts);
         }
         fon9::RevBufferList rts{64};
         fon9::RevPrint(rts, cStr);
         psymb->MdRtStream_.PublishAndSave(ToStrView(psymb->SymbId_),
                                           f9sv_RtsPackType_FieldValue_NoInfoTime,
                                           f9sv_MdRtsKind_Base,
                                           std::move(rts));
         break;
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
         this->OnSymbBaseChanged(*psymb, nullptr);
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
