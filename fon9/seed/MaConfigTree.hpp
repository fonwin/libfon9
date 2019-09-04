/// \file fon9/seed/MaConfigTree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_MaConfigTree_hpp__
#define __fon9_seed_MaConfigTree_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/seed/SeedSubr.hpp"
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/ConfigUtils.hpp"

namespace fon9 { namespace seed {

fon9_API void RevPrintConfigGridView(RevBuffer& rbuf, const Fields& flds,
                                     NamedSeedContainerImpl::iterator ibeg,
                                     NamedSeedContainerImpl::iterator iend);

class fon9_API MaConfigSeed;
class fon9_API MaConfigMgr;

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
   MaConfigMgr& ConfigMgr_;

   template <class... ArgsT>
   MaConfigTree(MaConfigMgr& configMgr, ArgsT&&... args)
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
fon9_WARN_POP;

class fon9_API MaConfigSeed : public NamedSeed {
   fon9_NON_COPY_NON_MOVE(MaConfigSeed);
   using base = NamedSeed;

   CharVector  Value_;
   void SetConfigValueImpl(const StrView& val) {
      this->Value_.assign(val);
   }

protected:
   /// 預設呼叫 this->SetConfigValueImpl(); 及 this->ConfigTree_.WriteConfig();
   virtual void OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val);

   friend class MaConfigTree; // 在 OnMaTree_AfterUpdated() 呼叫 OnConfigValueChanged();

public:
   MaConfigTree& ConfigTree_;

   using ConfigLocker = MaConfigTree::Locker;

   template <class... NamedArgsT>
   MaConfigSeed(MaConfigTree& configTree, NamedArgsT&&... namedargs)
      : base(std::forward<NamedArgsT>(namedargs)...)
      , ConfigTree_(configTree) {
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

class fon9_API MaConfigMgr : public NamedSapling {
   fon9_NON_COPY_NON_MOVE(MaConfigMgr);
   using base = NamedSapling;

public:
   template <class... NamedArgsT>
   MaConfigMgr(intrusive_ptr<MaConfigTree> sapling, NamedArgsT&&... namedargs)
      : base(std::move(sapling), std::forward<NamedArgsT>(namedargs)...) {
   }

   ~MaConfigMgr();

   MaConfigTree& GetConfigTree() const {
      assert(dynamic_cast<MaConfigTree*>(this->Sapling_.get()) != nullptr);
      return *static_cast<MaConfigTree*>(this->Sapling_.get());
   }

   /// MaConfigTree(Sapling_) 必須已包含所需的 MaConfigSeed;
   /// - 將載入結果置於 Description;
   /// - 將 cfgfn 置於 Title;
   /// - 必須在建構時呼叫, 或 Parent.lock 狀態,
   ///   否則 this->SetTitle(); this->SetDescription(); 不安全.
   void BindConfigFile(std::string cfgfn, bool isFireEvent);
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

} } // namespaces
#endif//__fon9_seed_MaConfigTree_hpp__
