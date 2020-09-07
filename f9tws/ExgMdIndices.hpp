// \file f9tws/ExgMdIndices.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdIndices_hpp__
#define __f9tws_ExgMdIndices_hpp__
#include "f9tws/Config.h"
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/fmkt/SymbTimePri.hpp"
#include "fon9/fmkt/MdRtStream.hpp"

namespace f9tws {

/// 交易所的指數資料, 只有時間 & 指數值.
class IndexDealData : public fon9::fmkt::SymbTimePri_Data {
   using base = fon9::fmkt::SymbTimePri_Data;
public:
   /// 市場行情序號.
   fon9::fmkt::MarketDataSeq  MarketSeq_{0};
   char  Padding____[4];

   void Clear() {
      base::Clear();
      this->MarketSeq_ = 0;
   }
};
typedef fon9::fmkt::SimpleSymbData<IndexDealData>  IndexDeal;
f9tws_API fon9::seed::Fields IndexDeal_MakeFields(bool isAddMarketSeq);
//--------------------------------------------------------------------------//
struct IndexRef_Data {
   fon9::fmkt::Pri   PriRef_{};

   void Clear() {
      fon9::ForceZeroNonTrivial(this);
   }
   bool operator==(const IndexRef_Data& rhs) const {
      return memcmp(this, &rhs, sizeof(*this)) == 0;
   }
   bool operator!=(const IndexRef_Data& rhs) const {
      return !this->operator==(rhs);
   }
};
typedef fon9::fmkt::SimpleSymbData<IndexRef_Data>  IndexRef;
f9tws_API fon9::seed::Fields IndexRef_MakeFields();
//--------------------------------------------------------------------------//
using IndexNameUTF8 = fon9::CharAry<88>;

class f9tws_API ExgMdIndex : public fon9::fmkt::Symb, public fon9::fmkt::SymbDataOHL {
   fon9_NON_COPY_NON_MOVE(ExgMdIndex);
   using base = fon9::fmkt::Symb;
public:
   /// 中文名稱.
   IndexNameUTF8           NameUTF8_{nullptr};
   IndexRef                Ref_;
   IndexDeal               Deal_;
   fon9::fmkt::MdRtStream  MdRtStream_;

   ExgMdIndex(const fon9::StrView& id, fon9::fmkt::MdRtStreamInnMgr& innMgr)
      : base{id}
      , MdRtStream_{innMgr} {
      this->TDayYYYYMMDD_ = innMgr.TDayYYYYMMDD();
      this->MdRtStream_.OpenRtStorage(*this);
   }
   ~ExgMdIndex();

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

   /// 台灣證券指數的換盤, 不應清除 SymbData(Open/High/Low...);
   /// - 設定 this->TradingSessionId_ = tsesId;
   /// - 設定 this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   /// - 不會觸發 this->GetSymbData(tabid=0..N)->OnSymbSessionClear();
   void SessionClear(fon9::fmkt::SymbTree& owner, f9fmkt_TradingSessionId tsesId) override;
   void DailyClear(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) override;
   void OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) override;

   static fon9::seed::LayoutSP MakeLayout(bool isAddMarketSeq);
};
//--------------------------------------------------------------------------//
class f9tws_API ExgMdIndices : public fon9::fmkt::MdSymbsT<ExgMdIndex> {
   fon9_NON_COPY_NON_MOVE(ExgMdIndices);
   using base = fon9::fmkt::MdSymbsT<ExgMdIndex>;

public:
   ExgMdIndices(std::string pathFmt, bool isAddMarketSeq);
   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;
};
using ExgMdIndicesSP = fon9::intrusive_ptr<ExgMdIndices>;

} // namespaces
#endif//__f9tws_ExgMdIndices_hpp__
