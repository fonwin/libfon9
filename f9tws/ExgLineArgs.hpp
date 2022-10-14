// \file f9tws/ExgLineArgs.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgLineArgs_hpp__
#define __f9tws_ExgLineArgs_hpp__
#include "f9tws/ExgTypes.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/CharAry.hpp"
#include "fon9/ConfigParser.hpp"

namespace f9tws {

struct f9tws_API ExgLineArgs {
   f9fmkt_TradingMarket Market_;
   BrkId                BrkId_;
   fon9::CharAryF<2>    SocketId_;
   fon9::CharAry<15>    PassKey_;
   uint16_t             PassNum_;
   /// Hb 的間隔(單位:秒), 預設為 0, 由各 session 根據交易所規範, 自行決定預設值.
   uint16_t             HbInterval_;

   void Clear();

   /// 不改變 args.Market_ 您必須自行處理.
   /// - tag = "BrkId=|SocketId=|Pass=|HbInt=";
   /// - tag = "SocketId" 與 "PvcId" 相同, 都會填入 SocketId_;
   /// - tag = "PassKey" 則必須自定 FnGetPassNum;
   fon9::ConfigParser::Result OnTagValue(fon9::StrView tag, fon9::StrView& value);

   /// 檢查欄位是否正確.
   std::string Verify() const;

   typedef uint16_t (*FnGetPassNum) (const ExgLineArgs& args);
   /// 預設: 不理會 args.PassKey_; 直接返回 args.PassNum_;
   /// 若須透過 fon9::auth::PassIdMgr 提供 args.PassKey_; 取得密碼, 則應自行設定 FnGetPassNum;
   static void SetFnGetPassNum(FnGetPassNum fnGetPassNum);

   uint16_t GetPassNum() const;
};

} // namespaces
#endif//__f9tws_ExgLineArgs_hpp__
