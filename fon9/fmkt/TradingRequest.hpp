/// \file fon9/fmkt/TradingRequest.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_TradingRequest_hpp__
#define __fon9_fmkt_TradingRequest_hpp__
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/intrusive_ref_counter.hpp"

namespace fon9 {
class RevBuffer;

namespace fmkt {

/// \ingroup fmkt
/// 回報序號, 每台主機自行依序編號.
/// 也就是 HostA.RxSNO=1, 與 HostB.RxSNO=1, 可能是不同的 TradingRxItem.
using TradingRxSNO = uint64_t;

/// \ingroup fmkt
/// 回報項目: 可能為「下單要求(新、刪、改、查...)」、「成交回報」、「委託異動」、「交易系統事件」...
class fon9_API TradingRxItem : public intrusive_ref_counter<TradingRxItem> {
   fon9_NON_COPY_NON_MOVE(TradingRxItem);

protected:
   f9fmkt_RxKind           RxKind_{};
   f9fmkt_TradingMarket    Market_{};
   f9fmkt_TradingSessionId SessionId_{};
   /// RxItemFlags_ 由衍生者自行決定如何使用.
   uint8_t                 RxItemFlags_{};
   TradingRxSNO            RxSNO_{0};

   virtual ~TradingRxItem();

public:
   TradingRxItem() = default;
   TradingRxItem(f9fmkt_RxKind rxKind) : RxKind_{rxKind} {
   }

   /// 必須透過 FreeThis() 來刪除, 預設 delete this;
   /// 若有使用 ObjCarrierTape 則將 this 還給 ObjCarrierTape;
   virtual void FreeThis();
   inline friend void intrusive_ptr_deleter(const TradingRxItem* p) {
      const_cast<TradingRxItem*>(p)->FreeThis();
   }

   f9fmkt_RxKind           RxKind() const { return this->RxKind_; }
   f9fmkt_TradingMarket    Market() const { return this->Market_; }
   f9fmkt_TradingSessionId SessionId() const { return this->SessionId_; }
   TradingRxSNO            RxSNO() const { return this->RxSNO_; }

   void SetMarket(f9fmkt_TradingMarket v) {
      assert(this->Market_ == f9fmkt_TradingMarket{});
      this->Market_ = v;
   }
   void SetSessionId(f9fmkt_TradingSessionId v) {
      assert(this->SessionId_ == f9fmkt_TradingSessionId{});
      this->SessionId_ = v;
   }
   /// 只能設定一次, 設定後不應該更改.
   TradingRxSNO SetRxSNO(TradingRxSNO sno) {
      assert(this->RxSNO_ == 0);
      return this->RxSNO_ = sno;
   }

   /// 將內容依照 Fields 的順序輸出到 rbuf;
   /// - 前方須包含 Name, 尾端 **不可加 '\n'**
   /// - 使用 fon9_kCSTR_CELLSPL 分隔欄位;
   /// - 不含 RxSNO();
   /// - 預設: do nothing.
   virtual void RevPrint(RevBuffer& rbuf) const;
   /// 預設傳回 nullptr.
   virtual const TradingRxItem* CastToRequest() const;
   /// 預設傳回 nullptr.
   virtual const TradingRxItem* CastToOrder() const;
   /// 預設傳回 nullptr.
   virtual const TradingRxItem* CastToEvent() const;
};
using TradingRxItemSP = intrusive_ptr<TradingRxItem>;

/// \ingroup fmkt
/// 下單要求基底.
class fon9_API TradingRequest : public TradingRxItem {
   fon9_NON_COPY_NON_MOVE(TradingRequest);
   using base = TradingRxItem;

protected:
   virtual ~TradingRequest();

public:
   using base::base;
   TradingRequest() = default;
};
using TradingRequestSP = intrusive_ptr<TradingRequest>;

} } // namespaces
#endif//__fon9_fmkt_TradingRequest_hpp__
