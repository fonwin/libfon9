// \file f9tws/ExgTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgTypes_hpp__
#define __f9tws_ExgTypes_hpp__
#include "f9tws/Config.h"
#include "fon9/CharAry.hpp"

namespace f9tws {

struct StkNo : public fon9::CharAryF<6, char, ' '> {
   using base = fon9::CharAryF<6, char, ' '>;
   using base::base;
   StkNo() = default;
};
constexpr fon9::StrView ToStrView(const StkNo& stkno) {
   return fon9::StrView{stkno.Chars_,
      stkno.Chars_[stkno.size() - 1] != ' ' ? stkno.size()
      : stkno.Chars_[stkno.size() - 2] != ' ' ? stkno.size() - 1
      : stkno.size() - 2};
}

//--------------------------------------------------------------------------//

using BrkId = fon9::CharAryF<4>;
using OrdNo = fon9::CharAryF<5>;

/// TSEC FIX 的 ClOrdID 固定使用 12 bytes, 若不足要補空白.
using ClOrdID = fon9::CharAryF<12, char, ' '>;

//--------------------------------------------------------------------------//

enum class TwsOType : char {
   /// 現股.
   Gn = '0',
   /// 代辦融資.
   CrAgent = '1',
   /// 代辦融券.
   DbAgent = '2',
   /// 自辦融資.
   CrSelf = '3',
   /// 自辦融券.
   DbSelf = '4',
   /// 借券賣出.
   SBL5 = '5',
   /// 避險套利借券賣出.
   SBL6 = '6',

   /// 現股當沖.
   DayTradeGn = 'a',
   /// 信用當沖.
   DayTradeCD = 'A',
};

enum class TwsApCode : char {
   Regular = '0',
   FixedPrice = '7',
   OddLot = '2',
};


} // namespaces
#endif//__f9tws_ExgTypes_hpp__
