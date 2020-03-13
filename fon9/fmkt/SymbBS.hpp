// \file fon9/fmkt/SymbBS.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBS_hpp__
#define __fon9_fmkt_SymbBS_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/SymbBSData.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 行情的買賣報價.
class fon9_API SymbBS : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbBS);
public:
   using Data = SymbBSData;
   enum {
      kBSCount = Data::kBSCount,
   };
   Data  Data_;
   
   SymbBS(const Data& rhs) : Data_{rhs} {
   }
   SymbBS() = default;

   void Clear(DayTime tm = DayTime::Null()) {
      memset(&this->Data_, 0, sizeof(this->Data_));
      this->Data_.InfoTime_ = tm;
   }
   void DailyClear() {
      this->Clear();
   }

   static seed::Fields MakeFields();
};

class fon9_API SymbBSTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbBSTabDy);
   using base = SymbDataTab;
public:
   SymbBSTabDy(Named&& named)
      : base{std::move(named), SymbBS::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbBS_hpp__
