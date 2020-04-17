// \file fon9/fmkt/SymbTws.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTws_hpp__
#define __fon9_fmkt_SymbTws_hpp__
#include "fon9/fmkt/SymbTwsBase.hpp"
#include "fon9/fmkt/Symb.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 台灣證券預設的基本商品類別.
/// - Symb + SymbTwsBase
class fon9_API SymbTws : public Symb, public SymbTwsBase {
   fon9_NON_COPY_NON_MOVE(SymbTws);
   using base = Symb;

public:
   using base::base;
   ~SymbTws();

   void OnSymbDailyClear(SymbTree& tree, const Symb& symb) override;
   /// 台灣證券的換盤, 不應清除 SymbData(Open/High/Low...);
   /// - 設定 this->TradingSessionId_ = tsesId;
   /// - 設定 this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   /// - 不會觸發 this->GetSymbData(tabid=0..N)->OnSymbSessionClear();
   void SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) override;

   /// base::MakeFields(); and:
   /// - ShUnit
   static seed::Fields MakeFields();
};
using SymbTwsSP = intrusive_ptr<SymbTws>;

} } // namespaces
#endif//__fon9_fmkt_SymbTws_hpp__
