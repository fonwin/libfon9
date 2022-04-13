// \file f9twf/TwfTickSize.cpp
// \author fonwinz@gmail.com
#include "f9twf/TwfTickSize.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/Log.hpp"

namespace f9twf {

using LvPriStepMap = fon9::SortedVector<ContractId, fon9::fmkt::LvPriStepVect>;
static LvPriStepMap LvPriStepMap_;

f9twf_API void Load_TwfTickSize(fon9::StrView defaultConfigPath, fon9::StrView cfgFileName) {
   try {
      fon9::ConfigLoader cfgld{defaultConfigPath.ToString()};
      cfgld.LoadFile(cfgFileName);
      fon9::StrView cfgs{&cfgld.GetCfgStr()};
      while (!cfgs.empty()) {
         fon9::StrView cfgln = fon9::StrFetchTrim(cfgs, '\n');
         if (cfgln.empty())
            continue;
         fon9::StrView id = fon9::StrFetchTrim(cfgln, '=');
         if (id.empty())
            continue;
         fon9::fmkt::LvPriStepVect lv;
         lv.FromStr(cfgln);
         if (lv.Get())
            LvPriStepMap_.kfetch(id).second = std::move(lv);
      }
   }
   catch (std::exception& e) {
      fon9_LOG_ERROR("Load_TwfTickSize:", e.what());
   }
}

f9twf_API const fon9::fmkt::LvPriStep* GetLvPriStep(ContractId contractId, f9fmkt_TradingMarket mkt, ExgContractType contractType) {
   auto ifind = LvPriStepMap_.find(contractId);
   if (ifind == LvPriStepMap_.end()) {
      contractId.Clear();
      contractId.Chars_[0] = static_cast<char>(mkt);
      contractId.Chars_[1] = '.';
      contractId.Chars_[2] = static_cast<char>(contractType);
      ifind = LvPriStepMap_.find(contractId);
      if (ifind == LvPriStepMap_.end())
         return nullptr;
   }
   return ifind->second.Get();
}

} // namespaces
