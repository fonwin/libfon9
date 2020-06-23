// \file f9tws/ExgMdFmt1Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt1Handler.hpp"
#include "f9tws/ExgMdFmt1.hpp"

namespace f9tws {
namespace f9fmkt = fon9::fmkt;

struct ExgMdFmt1Parser {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt1Parser);
   ExgMdSymbs::Locker SymbLk_;
   ExgMdSymb* const   Symb_;
   const f9fmkt_TradingMarket    PrevTradingMarket_;
   char                          Padding___[3];
   const uint32_t                PrevShUnit_;
   const f9fmkt::SymbRef::Data   PrevRefData_;
   ExgMdFmt1Parser(ExgMdSymbs& symbs, const ExgMdFmt1_C3& fmt1)
      : SymbLk_{symbs.SymbMap_.Lock()}
      , Symb_{static_cast<ExgMdSymb*>(symbs.FetchSymb(SymbLk_, ToStrView(fmt1.StkNo_)).get())}
      , PrevTradingMarket_{Symb_->TradingMarket_}
      , PrevShUnit_{Symb_->ShUnit_}
      , PrevRefData_(Symb_->Ref_.Data_) {
   }
   void ParseRef(const ExgMdFmt1_C3Ref& pk) {
      this->Symb_->Ref_.Data_.PriRef_.Assign<4>(fon9::PackBcdTo<uint64_t>(pk.PriRefV4_));
      this->Symb_->Ref_.Data_.PriUpLmt_.Assign<4>(fon9::PackBcdTo<uint64_t>(pk.PriUpLmtV4_));
      this->Symb_->Ref_.Data_.PriDnLmt_.Assign<4>(fon9::PackBcdTo<uint64_t>(pk.PriDnLmtV4_));
   }
   void ParseShUnit(const ExgMdFmt1_C3Tail& pk) {
      this->Symb_->ShUnit_ = fon9::PackBcdTo<uint32_t>(pk.TradingUnit_);
   }
   void Publish(ExgMdSystem& mdsys, const ExgMdHeader& pk, unsigned pksz) {
      if (this->PrevTradingMarket_ == this->Symb_->TradingMarket_
          && this->PrevShUnit_ == this->Symb_->ShUnit_
          && this->PrevRefData_ == Symb_->Ref_.Data_)
         return;
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
                                       fon9::DayTime::Null(),
                                       std::move(rbuf));
      // 基本資料有異動時儲存, 用在程式重啟時載入, 重建商品基本資料.
      // 以免重啟程式時, 因為沒有任何商品資料, 造成客戶端無法訂閱!
      mdsys.BaseInfoPkLog(pk, pksz);
   }
};

static bool CheckIsStockEntries(ExgMdSystem& mdsys, const ExgMdFmt1_C3& fmt1, unsigned pksz) {
   if (fmt1.StockEntries_[0] == ' ')
      return false;
   // 避免記錄重複的 "AL" or "NE", 需要額外處理是否有記錄過 "AL" or "NE";
   // 為了避免無謂的工作, 在此先將此記錄功能拿掉, 如果有需要再處理.
   // mdsys.BaseInfoPkLog(fmt1, pksz);
   (void)mdsys; (void)pksz;
   return true;
}

template <class Fmt1>
static void ParseFmt1(f9fmkt_TradingMarket mkt, ExgMdHandler& handler, const Fmt1& fmt1, unsigned pksz) {
   if (fon9_UNLIKELY(CheckIsStockEntries(handler.MdSys_, fmt1, pksz)))
      return;
   ExgMdFmt1Parser parser{*handler.MdSys_.Symbs_, fmt1};
   parser.Symb_->TradingMarket_ = mkt;
   parser.ParseRef(fmt1);
   parser.ParseShUnit(fmt1);
   parser.Publish(handler.MdSys_, fmt1, pksz);
}
//--------------------------------------------------------------------------//
ExgMdFmt1TwseHandler::~ExgMdFmt1TwseHandler() {
}
void ExgMdFmt1TwseHandler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   ParseFmt1(f9fmkt_TradingMarket_TwSEC, *this, *static_cast<const ExgMdFmt1v8_Twse*>(&pkhdr), pksz);
}
//--------------------------------------------------------------------------//
ExgMdFmt1TpexHandler::~ExgMdFmt1TpexHandler() {
}
void ExgMdFmt1TpexHandler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   ParseFmt1(f9fmkt_TradingMarket_TwOTC, *this, *static_cast<const ExgMdFmt1v8_Tpex*>(&pkhdr), pksz);
}

} // namespaces
