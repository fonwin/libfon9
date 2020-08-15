/// \file fon9/framework/IoManagerTree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_IoManagerTree_hpp__
#define __fon9_framework_IoManagerTree_hpp__
#include "fon9/framework/IoManager.hpp"
#include "fon9/ConfigFileBinder.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
class fon9_API IoManagerTree : public seed::Tree, public IoManager {
   fon9_NON_COPY_NON_MOVE(IoManagerTree);
   using baseTree = seed::Tree;
   using baseIoMgr = IoManager;
   unsigned IoManagerAddRef() override;
   unsigned IoManagerRelease() override;

public:
   /// 若 !args.CfgFileName_.empty()
   /// - afterOpen.IsNull() 載入設定檔, 但不啟動, 後續可透過 StartTimerForOpen() 啟動.
   /// - afterOpen <= 0: 載入設定檔, 立即在 TimerThread 啟動設定, 返回前可能已經啟動!
   /// - afterOpen > 0:  載入設定檔, 延遲 afterOpen 之後, 在 TimerThread 啟動設定.
   /// - 預設為 TimeInterval_Second(1);
   /// - 您必須確認設定的 session/device 不會用到尚未建構好的資源,
   ///   例如: IoManagerTree的衍生類尚未建構好, 但 session/device 已啟動想要使用衍生類.
   IoManagerTree(const IoManagerArgs& args, TimeInterval afterOpen = TimeInterval_Second(1));
   ~IoManagerTree();

   template <class IoTree = IoManagerTree, class... ArgsT>
   static intrusive_ptr<IoTree> Plant(seed::MaTree& maTree, const IoManagerArgs& ioargs, ArgsT&&... args) {
      intrusive_ptr<IoTree> retval{new IoTree(ioargs, std::forward<ArgsT>(args)...)};
      seed::NamedSapling*   seed;
      if (!maTree.Add(seed = new seed::NamedSapling(retval, ioargs.Name_)))
         return nullptr;
      seed->SetTitle(ioargs.CfgFileName_);
      seed->SetDescription(ioargs.Result_);
      return retval;
   }

   void OnTreeOp(seed::FnTreeOp fnCallback) override;
   void OnTabTreeOp(seed::FnTreeOp fnCallback) override;
   void OnParentSeedClear() override;

   /// 從 cfgfn 載入設定, 若後續設定有異動, 則會自訂將新的設定寫入 cfgfn.
   /// afterOpen: 請參考建構時 !args.CfgFileName_.empty() 的說明.
   /// retval.empty() 表示成功, 否則傳回錯誤訊息.
   std::string BindConfigFile(std::string cfgfn, TimeInterval afterOpen);
   /// 直接載入設定字串, 字串格式 = 設定檔內容.
   /// afterOpen: 請參考建構時 !args.CfgFileName_.empty() 的說明.
   /// retval.empty() 表示成功, 否則傳回錯誤訊息.
   std::string LoadConfigStr(StrView cfgstr, TimeInterval afterOpen);
   // 啟動Timer: n秒後檢查: Open or Close devices.
   void StartTimerForOpen(TimeInterval afterOpen);

   /// 銷毀全部的 devices 之後, 重新開啟.
   void DisposeAndReopen(std::string cause, TimeInterval afterReopen = TimeInterval_Second(3));

private:
   struct TreeOp;
   struct PodOp;
   static seed::LayoutSP MakeLayoutImpl();
   static seed::LayoutSP GetLayout();
   seed::TreeSP         TabTreeOp_; // for NeedsApply.
   seed::UnsafeSeedSubj StatusSubj_;
   ConfigFileBinder     ConfigFileBinder_;
   SubConn              SubConnDev_;
   SubConn              SubConnSes_;

   void NotifyChanged(DeviceItem&) override;
   void NotifyChanged(DeviceRun&) override;

   static void EmitOnTimer(TimerEntry* timer, TimeStamp now);
   using Timer = DataMemberEmitOnTimer<&IoManagerTree::EmitOnTimer>;
   Timer Timer_{GetDefaultTimerThread()};
   enum class TimerFor {
      Open,
      CheckSch,
   };
   TimerFor TimerFor_;
   void OnFactoryParkChanged();

   static void Apply(const seed::Fields& flds, StrView src, DeviceMapImpl& curmap, DeviceMapImpl& oldmap);
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_framework_IoManagerTree_hpp__
