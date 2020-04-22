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
using IndexDeal = fon9::fmkt::SymbTimePri;
f9tws_API fon9::seed::Fields IndexDeal_MakeFields();
//--------------------------------------------------------------------------//
class f9tws_API ExgMdIndex : public fon9::fmkt::Symb, public fon9::fmkt::SymbDataOHL {
   fon9_NON_COPY_NON_MOVE(ExgMdIndex);
   using base = fon9::fmkt::Symb;
public:
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
   void OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) override;

   static fon9::seed::LayoutSP MakeLayout();
};
//--------------------------------------------------------------------------//
class f9tws_API ExgMdIndices : public fon9::fmkt::MdSymbsT<ExgMdIndex> {
   fon9_NON_COPY_NON_MOVE(ExgMdIndices);
   using base = fon9::fmkt::MdSymbsT<ExgMdIndex>;

public:
   ExgMdIndices(std::string pathFmt);
   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;
};
using ExgMdIndicesSP = fon9::intrusive_ptr<ExgMdIndices>;

} // namespaces
#endif//__f9tws_ExgMdIndices_hpp__
