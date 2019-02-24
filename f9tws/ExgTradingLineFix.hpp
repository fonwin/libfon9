// \file f9tws/ExgTradingLineFix.hpp
//
// TSE/OTC FIX 下單線路.
//
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgTradingLineFix_hpp__
#define __f9tws_ExgTradingLineFix_hpp__
#include "f9tws/ExgTypes.hpp"
#include "fon9/fix/IoFixSession.hpp"
#include "fon9/fix/IoFixSender.hpp"
#include "fon9/fmkt/Trading.hpp"
#include "fon9/CharAry.hpp"
#include "fon9/FlowCounter.hpp"

namespace f9tws {
namespace f9fix = fon9::fix;
namespace f9fmkt = fon9::fmkt;

fon9_WARN_DISABLE_PADDING;
struct ExgTradingLineFixArgs {
   f9fmkt_TradingMarket Market_;
   BrkId                BrkId_;
   fon9::CharAry<2>     SocketId_;
   uint16_t             PassCode_;
   /// 流量管制: FcCount_(筆數) / FcTimeMS_(時間單位)
   /// 流量管制筆數(0=不限制).
   uint16_t             FcCount_;
   /// 流量管制, 時間單位, 1000=每秒.
   uint16_t             FcTimeMS_;
};
/// 不改變 args.Market_ 您必須自行處理.
/// cfg = "BrkId=|SocketId=|Pass=|Fc=筆數/ms";
/// 除了 "Fc=" 每個欄位都必須提供.
/// 若省略 "Fc=" 則表示 FcCount_=0; 若省略 "/ms" 則用設為 1000; 
/// retval.empty() 成功, retval = 失敗訊息.
f9tws_API std::string TwsFixArgParser(ExgTradingLineFixArgs& args, fon9::StrView cfg);

/// 建立適合 TSE/OTC 使用的 fixSender.
/// - 上市 retval->Initialize(recPath + "FIX44_XTAI_BrkId_SocketId.log");
/// - 上櫃 retval->Initialize(recPath + "FIX44_ROCO_BrkId_SocketId.log");
/// retval.empty() 成功, retval = 失敗訊息.
f9tws_API std::string MakeExgTradingLineFixSender(const ExgTradingLineFixArgs& args, fon9::StrView recPath, f9fix::IoFixSenderSP& out);

//--------------------------------------------------------------------------//

class f9tws_API ExgTradingLineFix : public f9fix::IoFixSession, public f9fmkt::TradingLine {
   fon9_NON_COPY_NON_MOVE(ExgTradingLineFix);
   using base = f9fix::IoFixSession;
   unsigned RawAppendNo_;

protected:
   fon9::FlowCounter FlowCounter_;

   /// 連線成功, 在此主動送出 Logon 訊息.
   void OnFixSessionConnected() override;
   f9fix::FixSenderSP OnFixSessionDisconnected(const fon9::StrView& info) override;

public:
   const ExgTradingLineFixArgs   LineArgs_;
   const f9fix::IoFixSenderSP    FixSender_;

   /// 建構前 fixSender->Initialize(fileName) 必須已經成功,
   /// 可考慮使用 MakeExgTradingLineFixSender() 建立 fixSender;
   ExgTradingLineFix(f9fix::IoFixManager&         mgr,
                     const f9fix::FixConfig&      fixcfg,
                     const ExgTradingLineFixArgs& lineargs,
                     f9fix::IoFixSenderSP&&       fixSender);
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_ExgTradingLineFix_hpp__
