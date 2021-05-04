// \file f9tws/ExgMdFmt1Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt1Handler.hpp"
#include "f9tws/ExgMdFmt1.hpp"
#include "fon9/Tools.hpp"

namespace f9tws {

ExgMdBaseInfoParser::ExgMdBaseInfoParser(ExgMdSymbs& symbs, const StkNo& stkNo)
   : Symbs_{symbs}
   , SymbLk_{symbs.SymbMap_.Lock()}
   , Symb_{static_cast<ExgMdSymb*>(symbs.FetchSymb(SymbLk_, ToStrView(stkNo)).get())}
   , PrevTradingMarket_{Symb_->TradingMarket_}
   , PrevShUnit_{Symb_->ShUnit_}
   , PrevRefData_(Symb_->Ref_.Data_)
   , PrevNameUTF8_{Symb_->NameUTF8_} {
}
void ExgMdBaseInfoParser::Publish(ExgMdSystem& mdsys, const ExgMdHeader& pk, unsigned pksz) const {
   if (this->PrevTradingMarket_ == this->Symb_->TradingMarket_
       && this->PrevShUnit_ == this->Symb_->ShUnit_
       && this->PrevRefData_ == this->Symb_->Ref_.Data_)
      return;
   fon9::Big5ToUtf8EOS(fon9::StrView_eos_or_all(static_cast<const ExgMdFmt1_C3*>(&pk)->ChineseName_, ' '),
                       this->Symb_->NameUTF8_.Chars_, sizeof(this->Symb_->NameUTF8_));
   if (mdsys.IsReloading())
      return;
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, this->Symb_->Ref_.Data_.PriDnLmt_);
   fon9::ToBitv(rbuf, this->Symb_->Ref_.Data_.PriUpLmt_);
   fon9::ToBitv(rbuf, this->Symb_->Ref_.Data_.PriRef_);
   fon9::ToBitv(rbuf, this->Symb_->ShUnit_);
   *rbuf.AllocPacket<char>() = this->Symb_->TradingMarket_;
   this->Symb_->MdRtStream_.Publish(ToStrView(this->Symb_->SymbId_),
                                    f9sv_RtsPackType_BaseInfoTw,
                                    f9sv_MdRtsKind_Base | f9sv_MdRtsKind_Ref,
                                    fon9::DayTime::Null(),
                                    std::move(rbuf));
   // 基本資料有異動時儲存, 用在程式重啟時載入, 重建商品基本資料.
   // 以免重啟程式時, 因為沒有任何商品資料, 造成客戶端無法訂閱!
   mdsys.BaseInfoPkLog(pk, pksz);

   if (this->PrevNameUTF8_ != this->Symb_->NameUTF8_) {
      fon9::fmkt::MdRtsPackFieldValue(rbuf, *this->Symbs_.FldBaseNameUTF8_,
                                      fon9::seed::SimpleRawRd{*this->Symb_});
      this->Symb_->MdRtStream_.PublishAndSave(ToStrView(this->Symb_->SymbId_),
                                              f9sv_RtsPackType_FieldValue_NoInfoTime,
                                              f9sv_MdRtsKind_Ref | f9sv_MdRtsKind_NoInfoTime,
                                              std::move(rbuf));
   }
}
//--------------------------------------------------------------------------//
using ExgMdFmt1Parser = ExgMdBaseInfoParser;
//--------------------------------------------------------------------------//
ExgMdFmt1TwseHandler::~ExgMdFmt1TwseHandler() {
}
void ExgMdFmt1TwseHandler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   EdgMdParseBaseInfo<ExgMdFmt1Parser>(f9fmkt_TradingMarket_TwSEC,
                                       *this, *this->MdSys_.Symbs_,
                                       *static_cast<const ExgMdFmt1v8_Twse*>(&pkhdr), pksz);
}
//--------------------------------------------------------------------------//
ExgMdFmt1TpexHandler::~ExgMdFmt1TpexHandler() {
}
void ExgMdFmt1TpexHandler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   EdgMdParseBaseInfo<ExgMdFmt1Parser>(f9fmkt_TradingMarket_TwOTC,
                                       *this, *this->MdSys_.Symbs_,
                                       *static_cast<const ExgMdFmt1v8_Tpex*>(&pkhdr), pksz);
}
//--------------------------------------------------------------------------//
ExgMdFmt1V9TwseHandler::~ExgMdFmt1V9TwseHandler() {
}
void ExgMdFmt1V9TwseHandler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   EdgMdParseBaseInfo<ExgMdBaseInfoParserV9>(f9fmkt_TradingMarket_TwSEC,
                                             *this, *this->MdSys_.Symbs_,
                                             *static_cast<const ExgMdFmt1v9_Twse*>(&pkhdr), pksz);
}
//--------------------------------------------------------------------------//
ExgMdFmt1V9TpexHandler::~ExgMdFmt1V9TpexHandler() {
}
void ExgMdFmt1V9TpexHandler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   EdgMdParseBaseInfo<ExgMdBaseInfoParserV9>(f9fmkt_TradingMarket_TwOTC,
                                             *this, *this->MdSys_.Symbs_,
                                             *static_cast<const ExgMdFmt1v9_Tpex*>(&pkhdr), pksz);
}

} // namespaces
