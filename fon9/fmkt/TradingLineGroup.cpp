/// \file fon9/fmkt/TradingLineGroup.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/TradingLineGroup.hpp"

namespace fon9 { namespace fmkt {

TradingLgMgrBase::~TradingLgMgrBase() {
}
TradingLgMgrBase::LgItemBaseSP TradingLgMgrBase::MakeLgItem(StrView tag, StrView cfgln) {
   if (tag.size() != fon9_kSIZE_LgTAG_LEAD + 1)
      return nullptr;
   const char* ptag = tag.begin();
   // Unknown tag.
   if (memcmp(ptag, fon9_kCSTR_LgTAG_LEAD, fon9_kSIZE_LgTAG_LEAD) != 0)
      return nullptr;
   const LgOut lgId = static_cast<LgOut>(ptag[fon9_kSIZE_LgTAG_LEAD]);
   // Unknown LgId.
   if (!IsValidateLgOut(lgId))
      return nullptr;
   const uint8_t lgIdx = LgOutToIndex(lgId);
   // Dup Lg.
   if (this->LgItems_[lgIdx].get() != nullptr)
      return nullptr;
   StrView  title = SbrFetchNoTrim(cfgln, ',');
   LgItemBaseSP retval = this->CreateLgItem(Named(
      tag.ToString(),
      StrView_ToNormalizeStr(StrTrimRemoveQuotes(title)),
      StrView_ToNormalizeStr(StrTrimRemoveQuotes(cfgln))
   ));
   this->LgItems_[lgIdx] = retval;
   this->Sapling_->Add(retval);
   return retval;
}

} } // namespaces
