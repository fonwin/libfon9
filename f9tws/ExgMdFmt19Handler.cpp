// \file f9tws/ExgMdFmt19Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt19Handler.hpp"
#include "f9tws/ExgMdFmt19.hpp"

namespace f9tws {
namespace f9fmkt = fon9::fmkt;

ExgMdFmt19Handler::~ExgMdFmt19Handler() {
}
void ExgMdFmt19Handler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   const ExgMdFmt19& fmt19 = *static_cast<const ExgMdFmt19*>(&pkhdr);
   // - 當日暫停交易時間與當日恢復交易時間記錄值均為"999999"時，
   //   該筆記錄之股票代號表示當次傳送之暫停/恢復交易股票數目，並且當次傳送作業結束。
   // - 股票代號為"000000"時，表示全市場為斷路階段。
   ///  => 直接使用 StkNo = "000000" 發行訊息.
   ///  => 若有需要「全市場斷路」的資訊, 則需訂閱 "000000",
   ///  => 或訂閱整棵樹時, 使用 Id="000000" 來判斷此訊息是「全市場斷路」的資訊.
   const unsigned suspendHHMMSS = fon9::PackBcdTo<unsigned>(fmt19.SuspendHHMMSS_);
   if (suspendHHMMSS == 999999)
      return;
   const unsigned resumeHHMMSS = fon9::PackBcdTo<unsigned>(fmt19.ResumeHHMMSS_);
   auto  symblk = this->MdSys_.Symbs_->SymbMap_.Lock();
   auto  symb = fon9::static_pointer_cast<ExgMdSymb>(
      this->MdSys_.Symbs_->FetchSymb(symblk, ToStrView(fmt19.StkNo_)));
   if (symb->BreakSt_.Data_.BreakHHMMSS_ == suspendHHMMSS
       && symb->BreakSt_.Data_.RestartHHMMSS_ == resumeHHMMSS)
      return;

   symb->BreakSt_.Data_.BreakHHMMSS_ = suspendHHMMSS;
   symb->BreakSt_.Data_.RestartHHMMSS_ = resumeHHMMSS;

   // -----
   fon9::seed::Tab*  tabBreakSt = this->MdSys_.Symbs_->TabBreakSt_;
   if (tabBreakSt == nullptr)
      return;
   fon9::RevBufferList rts{pksz};
   f9fmkt::MdRtsPackTabValues(rts, *tabBreakSt, symb->BreakSt_);
   symb->MdRtStream_.PublishAndSave(ToStrView(symb->SymbId_), f9fmkt::RtsPackType::TabValues, std::move(rts));
}

} // namespaces
