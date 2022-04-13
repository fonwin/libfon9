// \file f9twf/TwfTickSize.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_TwfTickSize_hpp__
#define __f9twf_TwfTickSize_hpp__
#include "fon9/fmkt/FmktTools.hpp"
#include "f9twf/ExgTypes.hpp"
#include "f9twf/Config.h"

namespace f9twf {

/// 使用 ConfigLoader 載入 TickSize 設定.
f9twf_API void Load_TwfTickSize(fon9::StrView defaultConfigPath = "", fon9::StrView cfgFileName = fon9::StrView("TwfTickSize.cfg"));

f9twf_API const fon9::fmkt::LvPriStep* GetLvPriStep(ContractId contractId, f9fmkt_TradingMarket mkt, ExgContractType contractType);

} // namespaces
#endif//__f9twf_TwfTickSize_hpp__
