/// \file fon9/seed/MaTreeConfig.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/MaTreeConfig.hpp"
#include "fon9/seed/ConfigGridView.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace seed {

fon9_API void RevPrintConfigGridView(RevBuffer& rbuf, const Fields& flds,
                                     NamedSeedContainerImpl::iterator ibeg,
                                     NamedSeedContainerImpl::iterator iend) {
   while (ibeg != iend) {
      RevPrintConfigFieldValues(rbuf, flds, SimpleRawRd{**--iend});
      RevPrint(rbuf, *fon9_kCSTR_ROWSPL, (*iend)->Name_);
   }
}

//--------------------------------------------------------------------------//

LayoutSP MaTreeConfigItem::MakeLayout(std::string tabName, TabFlag tabFlag) {
   Fields fields;
   fields.Add(fon9_MakeField2_const(MaTreeConfigItem, Title));
   fields.Add(fon9_MakeField2_const(MaTreeConfigItem, Description));
   fields.Add(fon9_MakeField2(MaTreeConfigItem, Value));
   return new Layout1(fon9_MakeField2(MaTreeConfigItem, Name),
                      new Tab(Named{std::move(tabName)}, std::move(fields), tabFlag));
}

//--------------------------------------------------------------------------//

void MaTreeConfig::OnMaTree_AfterUpdated(Locker& container, NamedSeed&) {
   if (!this->ConfigFileBinder_.HasBinding())
      return;
   RevBufferList rbuf{128};
   const Tab*    tab = this->LayoutSP_->GetTab(0);
   const auto&   flds = tab->Fields_;
   RevPrintConfigGridView(rbuf, flds, container->begin(), container->end());
   RevPrintConfigFieldNames(rbuf, flds, &this->LayoutSP_->KeyField_->Name_);
   std::string logHeader = "MaTreeConfig." + tab->Name_;
   this->ConfigFileBinder_.WriteConfig(&logHeader, BufferTo<std::string>(rbuf.MoveOut()));
}
std::string MaTreeConfig::BindConfigFile(std::string cfgfn) {
   std::string logHeader = "MaTreeConfig." + this->LayoutSP_->GetTab(0)->Name_;
   std::string res = this->ConfigFileBinder_.OpenRead(&logHeader, cfgfn);
   this->LoadConfigStr(&this->ConfigFileBinder_.GetConfigStr());
   return res;
}
void MaTreeConfig::LoadConfigStr(StrView cfgstr) {
   fon9::CharVector ordTeams;
   struct ToContainer : public GridViewToContainer {
      MaTreeBase::ContainerImpl* Container_;
      ToContainer(MaTreeBase::ContainerImpl* container) : Container_{container} {
      }
      void OnNewRow(StrView keyText, StrView ln) override {
         auto ifind = this->Container_->find(keyText);
         if (ifind != this->Container_->end()) {
            NamedSeed& val = **ifind;
            this->FillRaw(SimpleRawWr{val}, ln);
         }
      }
   };
   Locker      curmap{this->Container_};
   ToContainer toContainer{curmap.get()};
   toContainer.ParseConfigStr(this->LayoutSP_->GetTab(0)->Fields_, cfgstr);
}

} } // namespaces
