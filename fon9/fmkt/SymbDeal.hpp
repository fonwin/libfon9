// \file fon9/fmkt/SymbDeal.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbDeal_hpp__
#define __fon9_fmkt_SymbDeal_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/SymbDealData.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 行情的一筆成交資料.
fon9_API_TEMPLATE_CLASS(SymbDeal, SimpleSymbData, SymbDealData);

fon9_API seed::Fields SymbDeal_MakeFields(bool isAddLmtFlags, bool isAddMarketSeq);
inline seed::Fields SymbTwsDeal_MakeFields(bool isAddMarketSeq) {
   return SymbDeal_MakeFields(true, isAddMarketSeq);
}
inline seed::Fields SymbTwfDeal_MakeFields(bool isAddMarketSeq) {
   return SymbDeal_MakeFields(false, isAddMarketSeq);
}
inline seed::Fields SymbTwaDeal_MakeFields(bool isAddMarketSeq) {
   return SymbDeal_MakeFields(true, isAddMarketSeq);
}

class fon9_API SymbDealTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbDealTabDy);
   using base = SymbDataTab;
public:
   SymbDealTabDy(Named&& named, bool isAddMarketSeq = true)
      : base{std::move(named), SymbTwaDeal_MakeFields(isAddMarketSeq), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbDeal_hpp__
