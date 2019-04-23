// \file f9extests/SymbDy.cpp
// \author fonwinz@gmail.com
#include "f9extests/Config.h"
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/Plugins.hpp"

static bool SymbMap_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   // args = symbMapName.
   if (!fon9::StrTrim(&args).empty() && !fon9::ValidateName(args)) {
      holder.SetPluginsSt(fon9::LogLevel::Error, "Invalid SymbDy map name=", args);
      return false;
   }
   std::string          symbMapName = args.empty() ? "SymbDy" : args.ToString();
   fon9::seed::LayoutSP layout{new fon9::seed::LayoutDy(fon9_MakeField(fon9::Named{"Id"}, fon9::fmkt::Symb, SymbId_),
      fon9::seed::TabSP{new fon9::seed::Tab{fon9::Named{"Base"}, fon9::fmkt::Symb::MakeFields(), fon9::seed::TabFlag::NoSapling_NoSeedCommand_Writable}},
      fon9::seed::TreeFlag::AddableRemovable | fon9::seed::TreeFlag::Unordered)};
   fon9::fmkt::SymbDyTree* dy = new fon9::fmkt::SymbDyTree{std::move(layout)};
   if (holder.Root_->Add(new fon9::seed::NamedSapling(dy, std::move(symbMapName)))) {
      // 使用 SymbRef 驗證 SymbDy 機制.
      dy->AddSymbDataTab(new fon9::fmkt::SymbRefTabDy(fon9::Named{"Ref"}));
      dy->AddSymbDataTab(new fon9::fmkt::SymbBSTabDy(fon9::Named{"BS"}));
      dy->AddSymbDataTab(new fon9::fmkt::SymbDealTabDy(fon9::Named{"Deal"}));
   }
   return true;
}

extern "C" f9extests_API fon9::seed::PluginsDesc f9p_SymbDyMap;
fon9::seed::PluginsDesc f9p_SymbDyMap{"", &SymbMap_Start, nullptr, nullptr,};
