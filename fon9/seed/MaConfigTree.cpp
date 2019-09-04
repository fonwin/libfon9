/// \file fon9/seed/MaConfigTree.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/MaConfigTree.hpp"
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

MaConfigSeed::~MaConfigSeed() {
}
LayoutSP MaConfigSeed::MakeLayout(std::string tabName, TabFlag tabFlag) {
   Fields fields;
   fields.Add(fon9_MakeField2_const(MaConfigSeed, Title));
   fields.Add(fon9_MakeField2_const(MaConfigSeed, Description));
   fields.Add(fon9_MakeField2(MaConfigSeed, Value));
   return new Layout1(fon9_MakeField2(MaConfigSeed, Name),
                      new Tab(Named{std::move(tabName)}, std::move(fields), tabFlag));
}
void MaConfigSeed::OnConfigValueChanged(ConfigLocker&& lk, StrView val) {
   if (!lk.owns_lock())
      lk.lock();
   this->SetConfigValueImpl(val);
   this->ConfigTree_.WriteConfig(lk, this);
}

//--------------------------------------------------------------------------//

MaConfigTree::~MaConfigTree() {
}
void MaConfigTree::OnParentSeedClear() {
   SeedSubj_ParentSeedClear(this->SeedSubj_, *this);
   base::OnParentSeedClear();
}
void MaConfigTree::OnTreeOp(FnTreeOp fnCallback) {
   struct TreeCfgOp : public TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeCfgOp);
      using TreeOp::TreeOp;
      OpResult Subscribe(SubConn* pSubConn, Tab&, SeedSubr subr) override {
         *pSubConn = static_cast<MaConfigTree*>(&this->Tree_)->SeedSubj_.Subscribe(subr);
         return OpResult::no_error;
      }
      OpResult Unsubscribe(SubConn pSubConn) override {
         static_cast<MaConfigTree*>(&this->Tree_)->SeedSubj_.Unsubscribe(pSubConn);
         return OpResult::no_error;
      }
   };
   TreeCfgOp op{*this};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}
void MaConfigTree::UpdateConfigSeed(const Locker& lk, MaConfigSeed& item, std::string* title, std::string* description) {
   (void)lk; assert(lk.owns_lock());
   if (title)
      item.SetTitle(std::move(*title));
   if (description)
      item.SetDescription(std::move(*description));
   SeedSubj_Notify(this->SeedSubj_, *this, *this->LayoutSP_->GetTab(0), ToStrView(item.Name_), item);
}
void MaConfigTree::OnMaTree_AfterUpdated(Locker& container, NamedSeed& item) {
   if (auto cfgitem = dynamic_cast<MaConfigSeed*>(&item))
      cfgitem->OnConfigValueChanged(std::move(container), ToStrView(cfgitem->Value_));
   if (container.owns_lock())
      container.unlock();
   SeedSubj_Notify(this->SeedSubj_, *this, *this->LayoutSP_->GetTab(0), ToStrView(item.Name_), item);
}
void MaConfigTree::WriteConfig(const Locker& container, const MaConfigSeed* item) {
   assert(container.owns_lock());
   if (!this->ConfigFileBinder_.HasBinding() || this->IsFiringEvent_)
      return;
   RevBufferList rbuf{128};
   const Tab*    tab = this->LayoutSP_->GetTab(0);
   const auto&   flds = tab->Fields_;
   RevPrintConfigGridView(rbuf, flds, container->begin(), container->end());
   RevPrintConfigFieldNames(rbuf, flds, &this->LayoutSP_->KeyField_->Name_);
   std::string logHeader = "MaConfigTree." + tab->Name_;
   if (item) {
      logHeader.push_back('.');
      logHeader.append(item->Name_);
   }
   this->ConfigFileBinder_.WriteConfig(&logHeader, BufferTo<std::string>(rbuf.MoveOut()));
}
std::string MaConfigTree::BindConfigFile(std::string cfgfn, bool isFireEvent) {
   std::string logHeader = "MaConfigTree." + this->LayoutSP_->GetTab(0)->Name_;
   std::string res = this->ConfigFileBinder_.OpenRead(&logHeader, cfgfn);
   this->LoadConfigStr(&this->ConfigFileBinder_.GetConfigStr(), isFireEvent);
   return res;
}
void MaConfigTree::LoadConfigStr(StrView cfgstr, bool isFireEvent) {
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
   if (isFireEvent) {
      this->IsFiringEvent_ = true;
      for (size_t L = 0; L < curmap->size(); ++L) {
         this->OnMaTree_AfterUpdated(curmap, *curmap->sindex(L));
         if (!curmap.owns_lock())
            curmap.lock();
      }
      this->IsFiringEvent_ = false;
   }
}

//--------------------------------------------------------------------------//

MaConfigMgr::~MaConfigMgr() {
}
void MaConfigMgr::BindConfigFile(std::string cfgfn, bool isFireEvent) {
   this->SetDescription(this->GetConfigTree().BindConfigFile(cfgfn, isFireEvent));
   this->SetTitle(std::move(cfgfn));
}

} } // namespaces
