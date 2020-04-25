// \file f9extests/SymbIn.cpp
// \author fonwinz@gmail.com
#include "f9extests/SymbIn.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/Plugins.hpp"

namespace f9extests {

static const int32_t kSymbInOffset[]{
   0,
   fon9_OffsetOf(SymbIn, Ref_),
   fon9_OffsetOf(SymbIn, BS_),
   fon9_OffsetOf(SymbIn, Deal_),
};
static inline fon9::fmkt::SymbData* GetSymbInData(SymbIn* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kSymbInOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kSymbInOffset[tabid])
      : nullptr;
}

fon9::fmkt::SymbData* SymbIn::GetSymbData(int tabid) {
   return GetSymbInData(this, tabid);
}
fon9::fmkt::SymbData* SymbIn::FetchSymbData(int tabid) {
   return GetSymbInData(this, tabid);
}

fon9::seed::LayoutSP SymbIn::MakeLayout(fon9::seed::TreeFlag flags) {
   using namespace fon9::seed;
   return LayoutSP{new LayoutN(fon9_MakeField(Symb, SymbId_, "Id"), flags,
      TabSP{new Tab{fon9::Named{"Base"}, fon9::fmkt::Symb::MakeFields(),      TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{"Ref"},  fon9::fmkt::SymbRef_MakeFields(),    TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{"BS"},   fon9::fmkt::SymbTwaBS_MakeFields(),  TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{"Deal"}, fon9::fmkt::SymbTwaDeal_MakeFields(),TabFlag::NoSapling_NoSeedCommand_Writable}}
   )};
}

//--------------------------------------------------------------------------//

fon9::fmkt::SymbSP SymbInTree::MakeSymb(const fon9::StrView& symbid) {
   return fon9::fmkt::SymbSP{new SymbIn(symbid)};
}

static bool SymbMap_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   // args = symbMapName.
   if (!fon9::StrTrim(&args).empty() && !fon9::ValidateName(args)) {
      holder.SetPluginsSt(fon9::LogLevel::Error, "Invalid SymbIn map name=", args);
      return false;
   }
   std::string symbMapName = args.empty() ? "SymbIn" : args.ToString();
   holder.Root_->Add(new fon9::seed::NamedSapling(
      new SymbInTree{SymbIn::MakeLayout(fon9::seed::TreeFlag::AddableRemovable | fon9::seed::TreeFlag::Unordered)},
      std::move(symbMapName)));
   return true;
}
} // namespaces

extern "C" f9extests_API fon9::seed::PluginsDesc f9p_SymbInMap;
fon9::seed::PluginsDesc f9p_SymbInMap{"", &f9extests::SymbMap_Start, nullptr, nullptr, };
