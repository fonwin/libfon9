// \file f9tws/ExgMdSymbs.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdSymbs_hpp__
#define __f9tws_ExgMdSymbs_hpp__
#include "f9tws/Config.h"
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/fmkt/SymbTws.hpp"
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/fmkt/SymbTimePri.hpp"
#include "fon9/fmkt/SymbBreakSt.hpp"
#include "fon9/fmkt/MdRtStream.hpp"

namespace f9tws {

class f9tws_API ExgMdSymb : public fon9::fmkt::SymbTws, public fon9::fmkt::SymbDataOHL {
   fon9_NON_COPY_NON_MOVE(ExgMdSymb);
   using base = fon9::fmkt::SymbTws;
public:
   fon9::fmkt::SymbRef     Ref_;
   fon9::fmkt::SymbBS      BS_;
   fon9::fmkt::SymbDeal    Deal_;
   fon9::fmkt::SymbBreakSt BreakSt_;
   fon9::fmkt::MdRtStream  MdRtStream_;

   ExgMdSymb(const fon9::StrView& symbid, fon9::fmkt::MdRtStreamInnMgr& innMgr)
      : base{symbid}
      , MdRtStream_{innMgr} {
      this->TDayYYYYMMDD_ = innMgr.TDayYYYYMMDD();
      this->MdRtStream_.OpenRtStorage(*this);
   }
   ~ExgMdSymb();

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;
   /// 台灣證券的換盤, 不應清除 SymbData(Open/High/Low...);
   /// - 設定 this->TradingSessionId_ = tsesId;
   /// - 設定 this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   /// - 不會觸發 this->GetSymbData(tabid=0..N)->OnSymbSessionClear();
   void SessionClear(fon9::fmkt::SymbTree& owner, f9fmkt_TradingSessionId tsesId) override;

   /// 移除商品, 通常是因為商品下市.
   /// 預設觸發 this->MdRtStream_.BeforeRemove(owner, *this);
   void OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) override;

   static fon9::seed::LayoutSP MakeLayout();
};
//--------------------------------------------------------------------------//
class f9tws_API ExgMdSymbs : public fon9::fmkt::MdSymbsT<ExgMdSymb> {
   fon9_NON_COPY_NON_MOVE(ExgMdSymbs);
   using base = fon9::fmkt::MdSymbsT<ExgMdSymb>;

public:
   fon9::seed::Tab* const  TabBreakSt_;
   ExgMdSymbs(std::string rtiPathFmt);

   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;
};
using ExgMdSymbsSP = fon9::intrusive_ptr<ExgMdSymbs>;

} // namespaces
#endif//__f9tws_ExgMdSymbs_hpp__
