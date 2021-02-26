// \file f9tws/ExgTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgTradingLineMgr_hpp__
#define __f9tws_ExgTradingLineMgr_hpp__
#include "f9tws/ExgTradingLineFix.hpp"
#include "fon9/framework/IoManagerTree.hpp"

namespace f9tws {

fon9_WARN_DISABLE_PADDING;
class f9tws_API ExgTradingLineMgr
   : public f9fmkt::TradingLineManager // 負責管理: 可用線路、下單時尋找適當線路.
   , public f9fix::IoFixManager        // 負責處理: Fix線路可用時通知 f9fmkt::TradingLineManager
   , public fon9::IoManagerTree        // 負責管理: 通訊設定、狀態log、通訊物件的生死.
{        
   fon9_NON_COPY_NON_MOVE(ExgTradingLineMgr);
   using baseIo = fon9::IoManagerTree;
   using baseFix = f9fix::IoFixManager;

   fon9::TimeStamp            TDay_;
   f9fmkt_TradingSessionId    TradingSessionId_{f9fmkt_TradingSessionId_Normal};
   f9fmkt_TradingSessionSt    TradingSessionSt_{f9fmkt_TradingSessionSt_Unknown};
public:
   const f9fmkt_TradingMarket Market_;

   ExgTradingLineMgr(const fon9::IoManagerArgs& ioargs, fon9::TimeInterval afterOpen, f9fmkt_TradingMarket mkt)
      : baseIo(ioargs, afterOpen)
      , Market_(mkt) {
   }
   void OnFixSessionApReady(f9fix::IoFixSession& fixses) override;
   void OnFixSessionDisconnected(f9fix::IoFixSession& fixses, f9fix::FixSenderSP&& fixSender) override;

   /// 轉呼叫 this->OnBeforeDestroy(); 讓衍生者可以有機會清除排隊中的下單要求.
   void OnParentSeedClear() override;

   /// 交易日變更時通知, 必要時:
   /// - 在這裡會重建連線: this->DisposeAndReopen(std::move(cause));
   /// - 重設交易時段狀態: this->SetTradingSessionSt(f9fmkt_TradingSessionId_Normal, f9fmkt_TradingSessionSt_Unknown);
   void OnTDayChanged(fon9::TimeStamp tday, std::string cause);

   /// 設定現在交易時段狀態.
   /// FIX 送單時,需要判斷「盤中零股」or「盤後零股」.
   /// \retval false    tday < this->TDay(); 交易日不正確, 不改變交易時段狀態.
   bool SetTradingSessionSt(fon9::TimeStamp tday, f9fmkt_TradingSessionId sesId, f9fmkt_TradingSessionSt sesSt);

   fon9::TimeStamp TDay() const {
      return this->TDay_;
   }
   /// 是否在一般(早盤/盤中)交易時段內?
   bool IsNormalTrading() const {
      return this->TradingSessionId_ == f9fmkt_TradingSessionId_Normal
         && this->TradingSessionSt_ < f9fmkt_TradingSessionSt_Closed;
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_ExgTradingLineMgr_hpp__
