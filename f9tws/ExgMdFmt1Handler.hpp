// \file f9tws/ExgMdFmt1Handler.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt1Handler_hpp__
#define __f9tws_ExgMdFmt1Handler_hpp__
#include "f9tws/ExgMdSystem.hpp"

namespace f9tws {

class f9tws_API ExgMdFmt1TwseHandler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt1TwseHandler);
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmt1TwseHandler();
};

class f9tws_API ExgMdFmt1TpexHandler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt1TpexHandler);
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmt1TpexHandler();
};
//--------------------------------------------------------------------------//
struct ExgMdBaseInfoParser {
   fon9_NON_COPY_NON_MOVE(ExgMdBaseInfoParser);
   ExgMdSymbs::Locker SymbLk_;
   ExgMdSymb* const   Symb_;
   const f9fmkt_TradingMarket       PrevTradingMarket_;
   char                             Padding___[3];
   const uint32_t                   PrevShUnit_;
   const fon9::fmkt::SymbRef::Data  PrevRefData_;

   ExgMdBaseInfoParser(ExgMdSymbs& symbs, const StkNo& stkNo);

   template <class MdFmt>
   void ParseRef(const MdFmt& pk) {
      this->Symb_->Ref_.Data_.PriRef_.Assign<4>(fon9::PackBcdTo<uint64_t>(pk.PriRefV4_));
      this->Symb_->Ref_.Data_.PriUpLmt_.Assign<4>(fon9::PackBcdTo<uint64_t>(pk.PriUpLmtV4_));
      this->Symb_->Ref_.Data_.PriDnLmt_.Assign<4>(fon9::PackBcdTo<uint64_t>(pk.PriDnLmtV4_));
   }
   template <class MdFmt>
   void ParseShUnit(const MdFmt& pk) {
      this->Symb_->ShUnit_ = fon9::PackBcdTo<uint32_t>(pk.TradingUnit_);
   }

   void Publish(ExgMdSystem& mdsys, const ExgMdHeader& pk, unsigned pksz) const;
};

/// stockEntries = Fmt1.StockEntries_;
inline bool EdgMdFmt_IsStockEntries(ExgMdSystem& mdsys, const char (&stockEntries)[2], unsigned pksz) {
   if (stockEntries[0] == ' ')
      return false;
   // 避免記錄重複的 "AL" or "NE", 需要額外處理是否有記錄過 "AL" or "NE";
   // 為了避免無謂的工作, 在此先將此記錄功能拿掉, 如果有需要再處理.
   // mdsys.BaseInfoPkLog(fmt1, pksz);
   (void)mdsys; (void)pksz;
   return true;
}

template <class Parser, class MdFmt>
static inline void EdgMdParseBaseInfo(f9fmkt_TradingMarket mkt, ExgMdHandler& handler, ExgMdSymbs& symbs, const MdFmt& mdfmt, unsigned pksz) {
   if (fon9_UNLIKELY(EdgMdFmt_IsStockEntries(handler.MdSys_, mdfmt.StockEntries_, pksz)))
      return;
   Parser parser{symbs, mdfmt.StkNo_};
   parser.Symb_->TradingMarket_ = mkt;
   
   parser.ParseRef(mdfmt);
   parser.ParseShUnit(mdfmt);

   parser.Publish(handler.MdSys_, mdfmt, pksz);
}

} // namespaces
#endif//__f9tws_ExgMdFmt1Handler_hpp__
