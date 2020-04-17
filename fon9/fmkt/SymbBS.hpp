// \file fon9/fmkt/SymbBS.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBS_hpp__
#define __fon9_fmkt_SymbBS_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/SymbBSData.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 行情的買賣報價.
fon9_API_TEMPLATE_CLASS(SymbBS, SimpleSymbData, SymbBSData);

fon9_API seed::Fields SymbBS_MakeFields();

class fon9_API SymbBSTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbBSTabDy);
   using base = SymbDataTab;
public:
   SymbBSTabDy(Named&& named)
      : base{std::move(named), SymbBS_MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbBS_hpp__
