// \file f9tws/ExgMdFmt13.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt13_hpp__
#define __f9tws_ExgMdFmt13_hpp__
#include "f9tws/ExgMdFmt.hpp"

namespace f9tws {
fon9_PACK(1);

struct ExgMdFmt13v3 : public ExgMdHeader {
   StkNo       StkNo_;
   /// a.數值 < 143000 表示為試算揭示.
   /// b.數值 = 143000 表示為正式撮合成交揭示.
   /// c.數值 = 999999，StkNo_ = "000000" 表示當日零股交易撮合成交資料已全部送出。
   ExgMdHHMMSS Time_;
   bool IsLastDealSent() const {
      return this->Time_.HH_[0] == 0x99;
   }

   /// Bit 7-6 成交 00：一般成交或無成交 01：跌停成交 10：漲停成交.
   /// Bit 5-4 買進 00：一般買進或無買進價量 01：跌停買進 10：漲停買進.
   /// Bit 3-2 賣出 00：一般賣出或無賣出價量 01：跌停賣出 10：漲停賣出.
   /// Bit 1-0 保留位元.
   fon9::byte        LmtMask_;

   fon9::PackBcd<9>  TradingPriceV4_;
   /// 成交股數，單位為「股」.
   fon9::PackBcd<12> TradingVolume_;
   /// 揭示試算及正式撮合後未成交的最佳一檔買進價格，沒有未成交買進價格時傳送 0。
   fon9::PackBcd<9>  BuyPriceV4_;
   /// 揭示試算及正式撮合後未成交的最佳一檔賣出價格，沒有未成交賣出價格時傳送 0。
   fon9::PackBcd<9>  SellPriceV4_;
};
fon9_PACK_POP;

} // namespaces
#endif//__f9tws_ExgMdFmt13_hpp__
