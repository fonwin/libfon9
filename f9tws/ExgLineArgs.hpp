// \file f9tws/ExgLineArgs.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgLineArgs_hpp__
#define __f9tws_ExgLineArgs_hpp__
#include "f9tws/ExgTypes.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/CharAry.hpp"
#include "fon9/ConfigParser.hpp"

namespace f9tws {

fon9_WARN_DISABLE_PADDING;
struct f9tws_API ExgLineArgs {
   f9fmkt_TradingMarket Market_;
   BrkId                BrkId_;
   fon9::CharAryF<2>    SocketId_;
   uint16_t             PassCode_;
   /// Hb 的間隔(單位:秒), 預設為 0, 由各 session 根據交易所規範, 自行決定預設值.
   uint16_t             HbInterval_;

   void Clear();

   /// 不改變 args.Market_ 您必須自行處理.
   /// - tag = "BrkId=|SocketId=|Pass=|HbInt=";
   /// - tag = "SocketId" 與 "PvcId" 相同, 都會填入 SocketId_;
   fon9::ConfigParser::Result OnTagValue(fon9::StrView tag, fon9::StrView& value);

   /// 檢查欄位是否正確.
   std::string Verify() const;
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_ExgLineArgs_hpp__
