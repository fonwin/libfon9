// \file f9omstw/OmsCore.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsCore_hpp__
#define __f9omstw_OmsCore_hpp__
#include "f9omstw/OmsSymbTree.hpp"
#include "f9omstw/OmsBrkTree.hpp"
#include "fon9/seed/MaTree.hpp"
#include "fon9/MessageQueue.hpp"

namespace f9omstw {

using OmsThreadTask = std::function<void()>;
struct OmsThreadTaskHandler;
using OmsThread = fon9::MessageQueue<OmsThreadTaskHandler, OmsThreadTask>;

struct OmsThreadTaskHandler {
   using MessageType = OmsThreadTask;
   OmsThreadTaskHandler(OmsThread&);

   void OnMessage(OmsThreadTask& task) {
      task();
   }
   void OnThreadEnd(const std::string& threadName) {
      (void)threadName;
   }
};

/// OMS 所需的資源, 集中在此處理.
/// - 不同市場應建立各自的 OmsCore, 例如: 台灣證券的OmsCore, 台灣期權的OmsCore;
/// - 由衍生者自行初始化、啟動及結束:
///   - 建立 this->Symbs_;
///   - 建立 this->Brks_; 初始化 this->Brks_->Initialize();
///   - 將表格管理加入 Sapling, 啟動 Thread: this->Start();
///   - 解構前: this->WaitForEndNow();
class OmsCore : public fon9::seed::NamedSapling, public OmsThread {
   fon9_NON_COPY_NON_MOVE(OmsCore);

   friend struct OmsThreadTaskHandler;
   uintptr_t   ThreadId_{};

protected:
   using SymbTreeSP = fon9::intrusive_ptr<OmsSymbTree>;
   SymbTreeSP  Symbs_;

   using BrkTreeSP = fon9::intrusive_ptr<OmsBrkTree>;
   BrkTreeSP   Brks_;

   /// - 將 this->Symbs_; this->Brks_; 加入 this->Sapling.
   /// - 啟動 thread.
   void Start();

public:
   template <class... NamedArgsT>
   OmsCore(NamedArgsT&&... namedargs)
      : fon9::seed::NamedSapling(new fon9::seed::MaTree("Tables"), std::forward<NamedArgsT>(namedargs)...) {
   }

   void AddNamedSapling(fon9::seed::TreeSP sapling, fon9::Named&& named) {
      static_cast<fon9::seed::MaTree*>(this->Sapling_.get())->
         Add(new fon9::seed::NamedSapling(std::move(sapling), std::move(named)));
   }

   bool IsThisThread() const;
};

} // namespaces
#endif//__f9omstw_OmsCore_hpp__
