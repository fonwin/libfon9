/// \file fon9/seed/MaConfigTree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_MaConfigTree_hpp__
#define __fon9_seed_MaConfigTree_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/seed/SeedSubr.hpp"
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/ConfigUtils.hpp"
#include "fon9/SchTask.hpp"

namespace fon9 { namespace seed {

fon9_API void RevPrintConfigGridView(RevBuffer& rbuf, const Fields& flds,
                                     NamedSeedContainerImpl::iterator ibeg,
                                     NamedSeedContainerImpl::iterator iend);

class fon9_API MaConfigSeed;
class fon9_API MaConfigMgr;
class fon9_API MaConfigMgrBase;
using MaConfigMgrSP = intrusive_ptr<MaConfigMgr>;

fon9_WARN_DISABLE_PADDING;
class fon9_API MaConfigTree : public MaTree {
   fon9_NON_COPY_NON_MOVE(MaConfigTree);
   using base = MaTree;

   ConfigFileBinder  ConfigFileBinder_;
   bool              IsFiringEvent_{false};
   SeedSubj          SeedSubj_;

   void LoadConfigStr(StrView cfgstr, bool isFireEvent);

protected:
   void OnMaTree_AfterUpdated(Locker& container, NamedSeed& item) override;
   void OnParentSeedClear() override;

public:
   MaConfigMgrBase&  ConfigMgr_;

   template <class... ArgsT>
   MaConfigTree(MaConfigMgrBase& configMgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , ConfigMgr_(configMgr) {
   }

   ~MaConfigTree();

   void OnTreeOp(FnTreeOp fnCallback) override;
   /// 更新 item 的內容, 並觸發 SeedSubject 通知.
   void UpdateConfigSeed(const Locker& lk, MaConfigSeed& item, std::string* title, std::string* description);
   void UpdateConfigSeed(MaConfigSeed& item, std::string* title, std::string* description) {
      this->UpdateConfigSeed(this->Lock(), item, title, description);
   }

   /// MaConfigTree(Sapling_) 必須已包含所需的 MaConfigSeed;
   /// - 傳回失敗訊息.
   /// - 由 isFireEvent 決定: 設定載入後要不要觸發 OnMaTree_AfterUpdated(); 事件.
   std::string BindConfigFile(std::string cfgfn, bool isFireEvent);

   void WriteConfig(const Locker& container, const MaConfigSeed* item);
};
using MaConfigTreeSP = intrusive_ptr<MaConfigTree>;
fon9_WARN_POP;

class fon9_API MaConfigSeed : public NamedSeed {
   fon9_NON_COPY_NON_MOVE(MaConfigSeed);
   using base = NamedSeed;
   void Ctor();

protected:
   CharVector  Value_;
   void SetConfigValueImpl(const StrView& val) {
      this->Value_.assign(val);
   }

   /// 預設呼叫 this->SetConfigValueImpl(); 及 this->OwnerTree_.WriteConfig();
   virtual void OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val);

   friend class MaConfigTree; // 在 OnMaTree_AfterUpdated() 呼叫 OnConfigValueChanged();

public:
   MaConfigTree& OwnerTree_;

   using ConfigLocker = MaConfigTree::Locker;

   template <class... NamedArgsT>
   MaConfigSeed(MaConfigTree& configTree, NamedArgsT&&... namedargs)
      : base(std::forward<NamedArgsT>(namedargs)...)
      , OwnerTree_(configTree) {
      this->Ctor();
   }
   ~MaConfigSeed();

   static LayoutSP MakeLayout(std::string tabName, TabFlag tabFlag = TabFlag::NoSapling_NoSeedCommand_Writable);

   void SetConfigValue(ConfigLocker&& lk, const StrView& val) {
      assert(lk.owns_lock());
      this->SetConfigValueImpl(val);
      this->OnConfigValueChanged(std::move(lk), ToStrView(this->Value_));
   }
   StrView GetConfigValue(const ConfigLocker&) const {
      return ToStrView(this->Value_);
   }
};

class fon9_API MaConfigMgrBase {
   fon9_NON_COPY_NON_MOVE(MaConfigMgrBase);
   MaConfigTreeSP Sapling_;
protected:
   /// 衍生者必須在建構完成前設定.
   void SetConfigSapling(MaConfigTreeSP sapling) {
      assert(this->Sapling_.get() == nullptr);
      this->Sapling_ = std::move(sapling);
   }

public:
   MaConfigMgrBase() = default;
   virtual ~MaConfigMgrBase();

   MaConfigTree& GetConfigSapling() const {
      assert(this->Sapling_.get() != nullptr);
      return *this->Sapling_;
   }
   virtual StrView Name() const = 0;
   virtual void MaConfigMgr_AddRef() = 0;
   virtual void MaConfigMgr_Release() = 0;

   static void BindConfigFile(MaConfigMgrBase& rthis, NamedSeed& named, std::string cfgfn, bool isFireEvent);
};

class fon9_API MaConfigMgr : public NamedSeed, public MaConfigMgrBase {
   fon9_NON_COPY_NON_MOVE(MaConfigMgr);
   using SeedBase = NamedSeed;
   void MaConfigMgr_AddRef() override;
   void MaConfigMgr_Release() override;

public:
   template <class... NamedArgsT>
   MaConfigMgr(NamedArgsT&&... namedargs)
      : SeedBase(std::forward<NamedArgsT>(namedargs)...)
      , MaConfigMgrBase{} {
   }

   template <class... NamedArgsT>
   MaConfigMgr(LayoutSP layout, NamedArgsT&&... namedargs)
      : MaConfigMgr(std::forward<NamedArgsT>(namedargs)...) {
      this->SetConfigSapling(new MaConfigTree(*this, std::move(layout)));
   }

   ~MaConfigMgr();

   TreeSP GetSapling() override;
   StrView Name() const override;

   /// MaConfigTree(Sapling_) 必須已包含所需的 MaConfigSeed;
   /// - 將載入結果置於 Description;
   /// - 將 cfgfn 置於 Title;
   /// - 必須在建構時呼叫, 或 Parent.lock 狀態,
   ///   否則 this->SetTitle(); this->SetDescription(); 不安全.
   /// - isFireEvent 是否觸發 OnMaTree_AfterUpdated();
   ///   在 OnMaTree_AfterUpdated() 裡面會:
   ///   - 觸發 cfgitem->OnConfigValueChanged();
   ///   - 處發 SeedSubj_Notify();
   void BindConfigFile(std::string cfgfn, bool isFireEvent) {
      MaConfigMgrBase::BindConfigFile(*this, *this, std::move(cfgfn), isFireEvent);
   }
};

//--------------------------------------------------------------------------//
/// 「外部資料」的設定項目.
template <typename ExValueT>
class MaConfigSeed_ExValue : public MaConfigSeed {
   fon9_NON_COPY_NON_MOVE(MaConfigSeed_ExValue);
   using base = MaConfigSeed;
protected:
   ExValueT* const ExValue_;

   void OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val) override {
      *this->ExValue_ = StrTo(val, *this->ExValue_);
      NumOutBuf nbuf;
      base::OnConfigValueChanged(std::move(lk), StrView{ToStrRev(nbuf.end(), *this->ExValue_), nbuf.end()});
   }

public:
   using ExValueType = ExValueT;

   template <class... NamedArgsT>
   MaConfigSeed_ExValue(ExValueT* exValue, MaConfigTree& configTree, NamedArgsT&&... namedargs)
      : base(configTree, std::forward<NamedArgsT>(namedargs)...)
      , ExValue_{exValue} {
      assert(exValue != nullptr);
   }
};

using MaConfigSeed_YN = MaConfigSeed_ExValue<EnabledYN>;

//--------------------------------------------------------------------------//

/// 包含一個「主動排程器」的設定項目.
/// 處理 Sch 事件, 複寫: void OnSchTask_StateChanged(bool isInSch) override;
class fon9_API MaConfigSeed_SchTask : public MaConfigSeed, protected SchTask {
   fon9_NON_COPY_NON_MOVE(MaConfigSeed_SchTask);
   using base = MaConfigSeed;

protected:
   void OnParentTreeClear(Tree&) override;
   void OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val) override;
   virtual void OnSchCfgChanged(StrView cfgstr) = 0;

   void OnSchTask_NextCheckTime(const SchConfig::CheckResult&) override;
   void SetNextTimeInfo(TimeStamp tmNext, StrView exInfo);

public:
   using base::base;
   ~MaConfigSeed_SchTask();

   TimeStamp GetNextSchInTime();
};

} } // namespaces
#endif//__fon9_seed_MaConfigTree_hpp__
