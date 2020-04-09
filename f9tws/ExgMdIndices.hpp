// \file f9tws/ExgMdIndices.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdIndices_hpp__
#define __f9tws_ExgMdIndices_hpp__
#include "f9tws/Config.h"
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/fmkt/SymbHL.hpp"
#include "fon9/fmkt/MdRtStream.hpp"

namespace f9tws {

class f9tws_API IndexDeal : public fon9::fmkt::SymbData {
   fon9_NON_COPY_NON_MOVE(IndexDeal);
public:
   struct Data {
      /// 交易所的指數統計時間.
      fon9::DayTime  DealTime_{fon9::DayTime::Null()};
      /// 指數值.
      fon9::fmkt::Pri   DealPri_{};
   };
   Data  Data_;

   IndexDeal(const Data& rhs) : Data_{rhs} {
   }
   IndexDeal() = default;

   void DailyClear() {
      memset(&this->Data_, 0, sizeof(this->Data_));
      this->Data_.DealTime_.AssignNull();
   }

   static fon9::seed::Fields MakeFields();
};
//--------------------------------------------------------------------------//
class f9tws_API ExgMdIndex : public fon9::fmkt::Symb {
   fon9_NON_COPY_NON_MOVE(ExgMdIndex);
   using base = fon9::fmkt::Symb;
public:
   IndexDeal               Deal_;
   fon9::fmkt::SymbHigh    High_;
   fon9::fmkt::SymbLow     Low_;
   fon9::fmkt::MdRtStream  MdRtStream_;

   ExgMdIndex(const fon9::StrView& id, fon9::fmkt::MdRtStreamInnMgr& innMgr)
      : base{id}
      , MdRtStream_{innMgr} {
      this->TDayYYYYMMDD_ = innMgr.TDayYYYYMMDD();
   }

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

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
