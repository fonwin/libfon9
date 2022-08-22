// \file f9twf/ExgTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTypes_hpp__
#define __f9twf_ExgTypes_hpp__
#include "f9twf/Config.h"
#include "fon9/fmkt/FmktTypes.h"
#include "fon9/CharAryL.hpp"
#include "fon9/Utility.hpp"
#include "fon9/Decimal.hpp"
#include <array>

namespace f9twf {

struct ExgProdIdS {
   fon9::CharAry<10> ProdId_{nullptr};
};
struct ExgProdIdL {
   fon9::CharAry<20> ProdId_{nullptr};
};

using BrkId = fon9::CharAryF<7>;
using OrdNo = fon9::CharAryF<5>;
using SymbolId = fon9::CharAryL<sizeof(ExgProdIdL) * 2>; // 因應複式單, 所以長度應*2;
static_assert(sizeof(SymbolId) == 41, "SymbolId must pack?");

/// 契約代號.
using ContractId = fon9::CharAryP<4, 3, char, ' '>;
using StkNo = fon9::CharAryP<6, 4, char, ' '>;
using ContractSize = fon9::Decimal<uint64_t, 4>;

using FcmId = uint16_t;
using SessionId = uint16_t;

enum class ExgSystemType : uint8_t {
   /// 正常交易時段.選擇權.
   OptNormal = 10,
   /// 正常交易時段.期貨.
   FutNormal = 20,
   /// 盤後交易時段.選擇權.
   OptAfterHour = 11,
   /// 盤後交易時段.期貨.
   FutAfterHour = 21,
};
constexpr unsigned ExgSystemTypeCount() {
   return 4;
}
/// OptNormal=0; OptAfterHour=1; FutNormal=2; FutAfterHour=3;
/// else >= ExgSystemTypeCount();
constexpr uint8_t ExgSystemTypeToIndex(ExgSystemType val) {
   return static_cast<uint8_t>(fon9::cast_to_underlying(val) % 10
                          + (((fon9::cast_to_underlying(val) / 10) - 1) * 2));
}
static_assert(ExgSystemTypeToIndex(ExgSystemType::OptNormal) == 0
              && ExgSystemTypeToIndex(ExgSystemType::OptAfterHour) == 1
              && ExgSystemTypeToIndex(ExgSystemType::FutNormal) == 2
              && ExgSystemTypeToIndex(ExgSystemType::FutAfterHour) == 3,
              "ExgSystemTypeToIndex() error.");

constexpr bool ExgSystemTypeIsAfterHour(ExgSystemType val) {
   return (fon9::cast_to_underlying(val) % 10) == 1;
}
static inline f9fmkt_TradingMarket ExgSystemTypeToMarket(ExgSystemType val) {
   if (fon9::cast_to_underlying(val) <= 19)
      return f9fmkt_TradingMarket_TwOPT;
   if (fon9::cast_to_underlying(val) <= 29)
      return f9fmkt_TradingMarket_TwFUT;
   return f9fmkt_TradingMarket_Unknown;
}
static inline f9fmkt_TradingSessionId ExgSystemTypeToSessionId(ExgSystemType val) {
   switch (fon9::cast_to_underlying(val) % 10) {
   case 0: return f9fmkt_TradingSessionId_Normal;
   case 1: return f9fmkt_TradingSessionId_AfterHour;
   }
   return f9fmkt_TradingSessionId_Unknown;
}

enum class ExgPosEff : char {
   Open = 'O',
   Close = 'C',
   DayTrade = 'D',
   /// 報價.
   Quote = '9',
   /// 選擇權新倉含指定部位沖銷:A(Options Only)
   /// Open & specified position offsets.
   OptOpenAndClear = 'A',
   /// 代為沖銷.
   FcmForceClear = '7',
};

/// 複式商品買賣別.
enum class ExgCombSide : uint8_t {
   /// 不是複式單, 沒有第2隻腳.
   None = f9fmkt_SymbCombSide_None,
   /// Leg1.Side = Leg2.Side = 下單要求的買賣別.
   SameSide = f9fmkt_SymbCombSide_SameSide,
   /// Leg1.Side = 下單要求的買賣別;
   /// Leg2.Side = (Leg1.Side==Buy ? Sell : Buy);
   SideIsLeg1 = f9fmkt_SymbCombSide_SideIsLeg1,
   /// Leg2.Side = 下單要求的買賣別;
   /// Leg1.Side = (Leg2.Side==Buy ? Sell : Buy);
   SideIsLeg2 = f9fmkt_SymbCombSide_SideIsLeg2,
};

//--------------------------------------------------------------------------//

/// 契約類別.
enum class ExgContractType : char {
   /// I:指數類.
   Index = 'I',
   /// R:利率類.
   InterestRate = 'R',
   /// B:債券類.
   Bond = 'B',
   /// C:商品類.
   Commodity = 'C',
   /// E:匯率類.
   Currency = 'E',

   /// S:股票類:一般(P09.subtype_=='S' && P09.underlying_type_=='S').
   StockGen = 'S',
   /// e:股票類:ETF(P09.subtype_=='S' && P09.underlying_type_=='E').
   /// fon9 自訂, 若以後與期交所的 P09.subtype 衝突, 則需要調整.
   StockETF = 'e',
};
static inline bool ExgContractType_IsStock(ExgContractType st) {
   return(st == ExgContractType::StockGen || st == ExgContractType::StockETF);
}

/// 到期別: 標準 or 週;
enum class ExgExpiryType : char {
   Standard = 'S',
   Weekly = 'W',
};

/// 股票類契約 的 現貨類別.
/// E:ETF, S:個股.
enum class ExgUnderlyingType {
   ETF = 'E',
   Stock = 'S',
};

//--------------------------------------------------------------------------//

/// 台灣期交所 [商品年月] 編碼規則.
/// MY:
/// - M = 'A'..'L' = 1..12月(期貨 or 選擇權Call);
/// - M = 'M'..'X' = 1..12月(選擇權Put)
/// - Y = 西元年最後一碼 = '0'..'9';
struct CodeMY {
   char  MY_[2];

   /// \retval =0 MY碼有誤.
   /// \retval >0 +YMM: Call or Future;
   /// \retval <0 -YMM: Put;
   int ToYMM() const {
      uint8_t yy = static_cast<uint8_t>(this->MY_[1] - '0');
      if (fon9_LIKELY(yy < 10)) {
         uint8_t mm = static_cast<uint8_t>(this->MY_[0] - 'A');
         if (fon9_LIKELY(mm < 12))
            return (yy * 100) + (mm + 1);
         if (fon9_LIKELY(mm < 24))
            return (yy * (-100)) - (mm - 12 + 1);
      }
      return 0;
   }
   /// \retval =0 MY碼有誤.
   /// \retval YYYYMM; 例:
   ///         tdayYYYY=2018; this="A8"; 返回 201801;
   ///         tdayYYYY=2018; this="B9"; 返回 201902;
   ///         tdayYYYY=2019; this="C9"; 返回 201903;
   ///         tdayYYYY=2019; this="D0"; 返回 202004;
   unsigned ToYYYYMM(unsigned tdayYYYY) const {
      uint8_t  y1 = static_cast<uint8_t>(this->MY_[1] - '0');
      if (fon9_LIKELY(y1 < 10)) {
         uint8_t mm = static_cast<uint8_t>(this->MY_[0] - 'A');
         if (mm >= 12) { // IsOptPut?
            if ((mm = static_cast<uint8_t>(mm - 12)) >= 12)
               return 0;
         }
         unsigned yyy = (tdayYYYY / 10) + (y1 < (tdayYYYY % 10));
         return (((yyy * 10) + y1) * 100) + (mm + 1);
      }
      return 0;
   }
   bool IsOptPut() const {
      return this->MY_[0] >= static_cast<char>('A' + 12);
   }
   /// 期貨 or 買權格式: 月份碼 = 'A'..'L';
   /// \retval false mm 不是 1..12;
   /// \retval true 成功.
   bool FromYYYYMM(unsigned yyyymm, const char chMonStart = 'A') {
      unsigned mm = (yyyymm % 100) - 1;
      if (fon9_LIKELY(mm < 12)) {
         this->MY_[1] = static_cast<char>('0' + (yyyymm / 100) % 10);
         this->MY_[0] = static_cast<char>(chMonStart + mm);
         return true;
      }
      return false;
   }
   /// 賣權格式: 月份碼 = 'M'..'X';
   bool FromYYYYMM_OptPut(unsigned yyyymm) {
      return this->FromYYYYMM(yyyymm, 'M');
   }
};

//--------------------------------------------------------------------------//

/// PriLmts[lv], 例: lv == 0 使用 PriLmts_[0];
/// - <0=預告: -1 or -2...;
/// - >0=實施:  1 or  2...;
using TwfLvLmts = std::array<int8_t, 2>;

enum UDIdx : uint8_t {
   UDIdx_Up = 0,
   UDIdx_Dn = 1,
};

/// define: union { LvLmts_[]; struct { LvUpLmt_, LvDnLmt_ }};
#define f9twf_DEF_MEMBERS_LvLmts       \
fon9_MSC_WARN_DISABLE(4201);           \
union {                                \
   f9twf::TwfLvLmts  LvLmts_{{0,0}};   \
   struct {                            \
      int8_t         LvUpLmt_;         \
      int8_t         LvDnLmt_;         \
   };                                  \
};                                     \
fon9_MSC_WARN_POP

static inline uint8_t TwfGetLmtLv(int8_t lvContract, int8_t lvSymb) {
   if (fon9_UNLIKELY(lvContract < 0))                       // 預告,尚未實施.
      lvContract = static_cast<int8_t>((-lvContract) - 1);  // 所以使用前一檔.
   if (fon9_UNLIKELY(lvSymb < 0))
      lvSymb = static_cast<int8_t>((-lvSymb) - 1);
   return fon9::unsigned_cast(std::max(lvContract, lvSymb));
}

} // namespaces
#endif//__f9twf_ExgTypes_hpp__
