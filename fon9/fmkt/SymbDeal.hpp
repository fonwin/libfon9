// \file fon9/fmkt/SymbDeal.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbDeal_hpp__
#define __fon9_fmkt_SymbDeal_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/SymbDealData.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 行情的一筆成交資料.
class fon9_API SymbDeal : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbDeal);
public:
   using Data = SymbDealData;
   Data  Data_;

   SymbDeal(const Data& rhs) : Data_{rhs} {
   }
   SymbDeal() = default;

   void DailyClear() {
      memset(&this->Data_, 0, sizeof(this->Data_));
      this->Data_.InfoTime_.AssignNull();
      this->Data_.DealTime_.AssignNull();
   }
   DayTime DealTime() const {
      return this->Data_.DealTime_.IsNull() ? this->Data_.InfoTime_ : this->Data_.DealTime_;
   }

   static seed::Fields MakeFields();
};

class fon9_API SymbDealTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbDealTabDy);
   using base = SymbDataTab;
public:
   SymbDealTabDy(Named&& named)
      : base{std::move(named), SymbDeal::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbDeal_hpp__
