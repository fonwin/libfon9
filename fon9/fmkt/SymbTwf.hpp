// \file fon9/fmkt/SymbTwf.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTwf_hpp__
#define __fon9_fmkt_SymbTwf_hpp__
#include "fon9/fmkt/SymbTwfBase.hpp"
#include "fon9/fmkt/Symb.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 台灣期交所預設的基本商品類別.
/// - Symb + SymbTwfBase
class fon9_API SymbTwf : public Symb, public SymbTwfBase {
   fon9_NON_COPY_NON_MOVE(SymbTwf);
   using base = Symb;

public:
   using base::base;
   ~SymbTwf();

   bool IsExpired(unsigned tdayYYYYMMDD) const override;

   void SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) override;

   /// base::MakeFields(); and:
   /// - FlowGroup, PriceOrigDiv, StrikePriceDiv, ExgSymbSeq
   /// - EndDate
   static seed::Fields MakeFields();
};
using SymbTwfSP = intrusive_ptr<SymbTwf>;

} } // namespaces
#endif//__fon9_fmkt_SymbTwf_hpp__
