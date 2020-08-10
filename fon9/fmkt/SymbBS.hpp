// \file fon9/fmkt/SymbBS.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBS_hpp__
#define __fon9_fmkt_SymbBS_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/SymbBSData.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 行情的買賣報價.
fon9_API_TEMPLATE_CLASS(SymbTwsBS, SimpleSymbData, SymbTwsBSData);
fon9_API_TEMPLATE_CLASS(SymbTwfBS, SimpleSymbData, SymbTwfBSData);

/// 不包含「衍生買賣」, 包含 LmtFlags.
fon9_API seed::Fields SymbTwsBS_MakeFields(bool isAddMarketSeq);
/// 包含「衍生買賣」, 不包含 LmtFlags.
fon9_API seed::Fields SymbTwfBS_MakeFields(bool isAddMarketSeq);

using SymbTwaBS = SymbTwfBS;
fon9_API seed::Fields SymbTwaBS_MakeFields(bool isAddMarketSeq);

class fon9_API SymbBSTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbBSTabDy);
   using base = SymbDataTab;
public:
   SymbBSTabDy(Named&& named, bool isAddMarketSeq = true)
      : base{std::move(named), SymbTwaBS_MakeFields(isAddMarketSeq), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbBS_hpp__
