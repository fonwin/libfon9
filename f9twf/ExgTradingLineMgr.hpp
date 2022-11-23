// \file f9twf/ExgTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTradingLineMgr_hpp__
#define __f9twf_ExgTradingLineMgr_hpp__
#include "f9twf/ExgLineTmpSession.hpp"
#include "f9twf/ExgMapMgr.hpp"
#include "fon9/fmkt/TradingLine.hpp"
#include "fon9/framework/IoManagerTree.hpp"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

/// 負責管理: 可用線路、下單時尋找適當線路.
/// - 管理一組相同 ExgSystemType 的「交易線路」
/// - 日盤、夜盤時間會有重疊, 使用各自的 TradingLineMgr;
///   - 收到下單要求時, 根據商品的 FlowGroup 判斷交易時段.
///   - 每個 FlowGroup 有自己的交易時段.
/// - 當連線成功後, 訊息補完後.
///   - 若是「交易線路」ApCode = Trading(4) 或 TradingShortFormat(6)
///   - 則進入可送單狀態, 此時會到 OnTradingLineReady() 註冊.
class f9twf_API ExgTradingLineMgr : public f9fmkt::TradingLineManager
                                  , public fon9::IoManagerTree {
   fon9_NON_COPY_NON_MOVE(ExgTradingLineMgr);
   using baseTrading = f9fmkt::TradingLineManager;
   using baseIo = fon9::IoManagerTree;

public:
   const ExgMapMgrSP    ExgMapMgr_;
   const ExgSystemType  ExgSystemType_;
   char                 Padding7____[7];

   ExgTradingLineMgr(const fon9::IoManagerArgs& ioargs, fon9::TimeInterval afterOpen,
                     ExgMapMgrSP exgMapMgr, ExgSystemType systemType)
      : baseIo(ioargs, afterOpen)
      , ExgMapMgr_{std::move(exgMapMgr)}
      , ExgSystemType_{systemType} {
   }

   bool AppendDn(const ExgLineTmpArgs& lineArgs, std::string& devcfg) const {
      return this->ExgMapMgr_->AppendDn(this->ExgSystemType_, lineArgs, devcfg);
   }

   /// 清除排隊中的下單要求.
   void OnParentSeedClear() override;

   /// 交易日變更時通知, 必要時在這裡會重建連線: this->DisposeAndReopen(std::move(cause));
   void OnTDayChanged(fon9::TimeStamp tday, std::string cause);
};

} // namespaces
#endif//__f9twf_ExgTradingLineMgr_hpp__
