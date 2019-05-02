// \file tws/TwsSymb.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_tws_TwsSymb_hpp__
#define __f9omstw_tws_TwsSymb_hpp__
#include "fon9/fmkt/Symb.hpp"

namespace f9omstw {

/// 這裡提供一個 OMS 商品基本資料的範例.
/// 由繼承而得到的欄位: 一張的單位數(股數): ShUnit_
class TwsSymb : public fon9::fmkt::Symb {
   fon9_NON_COPY_NON_MOVE(TwsSymb);
   using base = fon9::fmkt::Symb;
public:
   using base::base;

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags);

   static fon9::fmkt::SymbSP SymbMaker(const fon9::StrView& symbid) {
      return new TwsSymb{symbid};
   }
};

} // namespaces
#endif//__f9omstw_tws_TwsSymb_hpp__
