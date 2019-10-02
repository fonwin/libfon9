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

class f9twf_API ExgIoManager;

fon9_WARN_DISABLE_PADDING;
/// 負責管理: 可用線路、下單時尋找適當線路.
/// - 管理一組相同 ExgSystemType 的「交易線路」
/// - ExgSystemType 在線路申請時就決定, 然後在 L30(通知登錄訊息) 提供.
/// - 當連線成功後, 訊息補完後.
///   - 若是「交易線路」ApCode = Trading(4) 或 TradingShortFormat(6)
///   - 則進入可送單狀態, 此時會到 OnTradingLineReady() 註冊.
class f9twf_API ExgTradingLineMgr : public f9fmkt::TradingLineManager {
   fon9_NON_COPY_NON_MOVE(ExgTradingLineMgr);
   using base = f9fmkt::TradingLineManager;
   friend class f9twf_API ExgIoManager; // this->OnBeforeDestroy();

public:
   ExgIoManager& IoManager_;
   ExgTradingLineMgr(ExgIoManager& ioMgr) : IoManager_(ioMgr) {
   }
};

/// - 線路申請時就已決定好線路的 SystemType;
/// - 日盤、夜盤時間會有重疊: 所以使用各自的 TradingLineMgr;
///   - 收到下單要求時, 根據商品的 FlowGroup 判斷交易時段.
///   - 每個 FlowGroup 有自己的交易時段.
class f9twf_API ExgIoManager : public fon9::IoManagerTree {
   fon9_NON_COPY_NON_MOVE(ExgIoManager);
   using base = fon9::IoManagerTree;

public:
   const ExgMapMgrSP ExgMapMgr_;
   /// 日盤線路管理.
   ExgTradingLineMgr RegularLineMgr_;
   /// 夜盤線路管理.
   ExgTradingLineMgr AfterHourLineMgr_;

   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list.
   ExgIoManager(ExgMapMgrSP exgMapMgr, const fon9::IoManagerArgs& ioargs, fon9::TimeInterval afterOpen)
      : base(ioargs, afterOpen)
      , ExgMapMgr_{std::move(exgMapMgr)}
      , RegularLineMgr_(*this)
      , AfterHourLineMgr_(*this) {
   }
   fon9_MSC_WARN_POP;

   /// 清除排隊中的下單要求.
   /// - this->RegularLineMgr_.OnBeforeDestroy();
   /// - this->AfterHourLineMgr_.OnBeforeDestroy();
   void OnParentSeedClear() override;
};
fon9_WARN_POP;

} // namespaces
#endif//__f9twf_ExgTradingLineMgr_hpp__
