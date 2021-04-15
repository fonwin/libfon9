/// \file fon9/seed/MaConfigTree.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/MaConfigTree.hpp"
#include "fon9/seed/ConfigGridView.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

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

void MaConfigSeed::Ctor() {
   this->OwnerTree_.ConfigMgr_.MaConfigMgr_AddRef();
}
MaConfigSeed::~MaConfigSeed() {
   this->OwnerTree_.ConfigMgr_.MaConfigMgr_Release();
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
   this->OwnerTree_.WriteConfig(lk, this);
}

//--------------------------------------------------------------------------//

MaConfigTree::~MaConfigTree() {
}
void MaConfigTree::OnParentSeedClear() {
   {
      auto treelk = this->Lock();
      SeedSubj_ParentSeedClear(this->TreeSubj_, *this);
   }
   base::OnParentSeedClear();
}
void MaConfigTree::OnTreeOp(FnTreeOp fnCallback) {
   struct TreeCfgOp : public TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeCfgOp);
      using TreeOp::TreeOp;
      OpResult Subscribe(SubConn* pSubConn, Tab& tab, FnSeedSubr subr) override {
         return static_cast<MaConfigTree*>(&this->Tree_)->TreeSubj_.SafeSubscribe(
            static_cast<MaConfigTree*>(&this->Tree_)->Lock(), *this, pSubConn, tab, subr);
      }
      OpResult Unsubscribe(SubConn* pSubConn, Tab& tab) override {
         (void)tab;
         return static_cast<MaConfigTree*>(&this->Tree_)->TreeSubj_.SafeUnsubscribe(
            static_cast<MaConfigTree*>(&this->Tree_)->Lock(), pSubConn);
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
   SeedSubj_Notify(this->TreeSubj_, *this, *this->LayoutSP_->GetTab(0), ToStrView(item.Name_), item);
}
void MaConfigTree::OnMaTree_AfterUpdated(Locker& container, NamedSeed& item) {
   if (auto cfgitem = dynamic_cast<MaConfigSeed*>(&item))
      cfgitem->OnConfigValueChanged(std::move(container), ToStrView(cfgitem->Value_));
   if (!container.owns_lock())
      container.lock();
   SeedSubj_Notify(this->TreeSubj_, *this, *this->LayoutSP_->GetTab(0), ToStrView(item.Name_), item);
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

MaConfigMgrBase::~MaConfigMgrBase() {
}
void MaConfigMgrBase::BindConfigFile(MaConfigMgrBase& rthis, NamedSeed& named, std::string cfgfn, bool isFireEvent) {
   named.SetDescription(rthis.GetConfigSapling().BindConfigFile(cfgfn, isFireEvent));
   named.SetTitle(std::move(cfgfn));
}

MaConfigMgr::~MaConfigMgr() {
}
TreeSP MaConfigMgr::GetSapling() {
   return &this->GetConfigSapling();
}
StrView MaConfigMgr::Name() const {
   return &this->Name_;
}
void MaConfigMgr::MaConfigMgr_AddRef() {
   intrusive_ptr_add_ref(this);
}
void MaConfigMgr::MaConfigMgr_Release() {
   intrusive_ptr_release(this);
}

//--------------------------------------------------------------------------//

MaConfigSeed_SchTask::~MaConfigSeed_SchTask() {
}
void MaConfigSeed_SchTask::OnParentTreeClear(Tree& tree) {
   this->DisposeAndWait();
   base::OnParentTreeClear(tree);
}
void MaConfigSeed_SchTask::OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val) {
   CharVector cfgstr{val};
   lk.unlock();
   this->OnSchCfgChanged(ToStrView(cfgstr));
   // ----- 取得 Sch設定 的正規化字串.
   {
      SchImpl::Locker schImpl{this->SchImpl_};
      cfgstr = RevPrintTo<CharVector>(schImpl->Config_);
   }
   // -----
   base::OnConfigValueChanged(std::move(lk), ToStrView(cfgstr));
}
TimeStamp MaConfigSeed_SchTask::GetNextSchInTime() {
   SchImpl::Locker schImpl{this->SchImpl_};
   return schImpl->Config_.GetNextSchIn(UtcNow(), schImpl->SchSt_);
}
void MaConfigSeed_SchTask::SetNextTimeInfo(TimeStamp tmNext, StrView exInfo) {
   RevBufferFixedSize<sizeof(NumOutBuf) * 2>  strNextTime;
   if (!tmNext.IsNullOrZero())
      RevPrint(strNextTime, "|Next=", tmNext, FmtTS{"f-T+'L'"});

   std::string desc = RevPrintTo<std::string>(LocalNow(), FmtTS{"f-T.6"}, '|', exInfo, strNextTime);
   auto locker{this->OwnerTree_.Lock()};
   fon9_LOG_INFO("FileImpMgr|Info=", exInfo,
                 "|Name=", this->OwnerTree_.ConfigMgr_.Name(), '/', this->Name_,
                 strNextTime,
                 "|SchCfg={", this->GetConfigValue(locker), '}');
   this->OwnerTree_.UpdateConfigSeed(locker, *this, nullptr, &desc);
}
void MaConfigSeed_SchTask::OnSchTask_NextCheckTime(const SchConfig::CheckResult&) {
   this->SetNextTimeInfo(this->GetNextSchInTime(), "NextCheckTime");
}

} } // namespaces
