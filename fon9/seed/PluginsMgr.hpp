/// \file fon9/seed/PluginsMgr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_PluginsMgr_hpp__
#define __fon9_seed_PluginsMgr_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/ConfigUtils.hpp"

namespace fon9 { namespace seed {

fon9_MSC_WARN_DISABLE(4355);/*'this' : used in base member initializer list)*/
/// \ingroup seed
/// 負責管理一組 Plugins.
/// - 可綁設定檔: 儲存、載入.
/// - 在特定 thread 啟動、操作 plugins.
class fon9_API PluginsMgr : public NamedSapling {
   fon9_NON_COPY_NON_MOVE(PluginsMgr);
   using base = NamedSapling;
   static TreeSP MakePluginsSapling(PluginsMgr&, MaTreeSP&& root);
   struct PluginsTree;
   using PluginsTreeSP = TreeSPT<PluginsTree>;
public:
   template <class... NamedArgsT>
   PluginsMgr(MaTreeSP root, NamedArgsT&&... namedargs)
      : base(MakePluginsSapling(*this, std::move(root)),
             std::forward<NamedArgsT>(namedargs)...) {
   }
   /// retval.empty() 表示成功, 否則傳回錯誤訊息.
   /// 返回前設定:
   /// - this->SetTitle(cfgfn);
   /// - this->SetDescription(retval);
   std::string BindConfigFile(StrView cfgfn);

   /// 直接載入設定字串, 字串格式 = 設定檔內容.
   /// retval.empty() 表示成功, 否則傳回錯誤訊息.
   std::string LoadConfigStr(StrView cfgstr);

   struct PluginsRec;
};
using PluginsMgrSP = intrusive_ptr<PluginsMgr>;
fon9_MSC_WARN_POP;

} } // namespaces
#endif//__fon9_seed_PluginsMgr_hpp__
