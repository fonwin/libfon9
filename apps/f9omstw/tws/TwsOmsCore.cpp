// \file tws/TwsOmsCore.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsCore.hpp"
#include "fon9/seed/Plugins.hpp"
#include "tws/TwsSymb.hpp"
#include "tws/TwsBrk.hpp"

namespace f9omstw {

struct TwsOmsCore : public OmsCore {
   fon9_NON_COPY_NON_MOVE(TwsOmsCore);
   using base = OmsCore;
   TwsOmsCore() : base{"omstws"} {
      this->Symbs_.reset(new OmsSymbTree(*this, TwsSymb::MakeLayout(OmsSymbTree::DefaultTreeFlag()), &TwsSymb::SymbMaker));
      this->Brks_.reset(new OmsBrkTree(*this, TwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
      this->Brks_->Initialize(&TwsBrk::BrkMaker, "8610", 5u, &IncStrAlpha);
      this->Start();
   }
   ~TwsOmsCore() {
      this->WaitForEndNow();
   }
};

static bool TwsOmsCore_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   (void)args;
   holder.Root_->Add(new TwsOmsCore{});
   return true;
}

} // namespaces

static fon9::seed::PluginsDesc f9p_TwsOmsCore{"", &f9omstw::TwsOmsCore_Start, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"TwsOmsCore", &f9p_TwsOmsCore};
