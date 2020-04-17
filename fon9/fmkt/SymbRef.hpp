// \file fon9/fmkt/SymbRef.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbRef_hpp__
#define __fon9_fmkt_SymbRef_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace fon9 { namespace fmkt {

struct SymbRef_Data {
   Pri   PriRef_{};
   Pri   PriUpLmt_{};
   Pri   PriDnLmt_{};

   void Clear() {
      memset(this, 0, sizeof(*this));
   }
   bool operator==(const SymbRef_Data& rhs) const {
      return memcmp(this, &rhs, sizeof(*this)) == 0;
   }
   bool operator!=(const SymbRef_Data& rhs) const {
      return !this->operator==(rhs);
   }
};

/// \ingroup fmkt
/// 商品資料的擴充: 參考價.
fon9_API_TEMPLATE_CLASS(SymbRef, SimpleSymbData, SymbRef_Data);

fon9_API seed::Fields SymbRef_MakeFields();

class fon9_API SymbRefTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbRefTabDy);
   using base = SymbDataTab;
public:
   SymbRefTabDy(Named&& named)
      : base{std::move(named), SymbRef_MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbRef_hpp__
