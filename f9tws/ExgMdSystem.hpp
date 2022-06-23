// \file f9tws/ExgMdSystem.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdSystem_hpp__
#define __f9tws_ExgMdSystem_hpp__
#include "f9tws/ExgMdSystemBase.hpp"
#include "f9tws/ExgMdSymbs.hpp"
#include "f9tws/ExgMdIndices.hpp"
#include "fon9/FileAppender.hpp"

namespace f9tws {

/// f9tws::ExgMdSystem: for TWSE/TPEX Md Rts system;
/// TwsMdSys
///   ├─ Symbs      商品資料表.
///   ├─ Indices    指數資料表.
///   └─ ExgMdIoMgr 資訊來源: ExgMdIoMgr 負責管理資料的連續性,
///                 所以「主、備」必須設定在同一個 ExgMdIoMgr.
///
/// 系統配置方式, 底下方式擇一:
/// 1. 上市上櫃合併資料表, 建立1個 TwsMdSys, 例: TwsMd;
///    - 降低系統管理的複雜度, 但可能因搶鎖而稍微增加延遲。
///    - 訂閱者不必理會商品的市場。
///    - 使用者若有訂閱整棵樹的權限, 則一旦訂閱 "/TwsMd/Symbs",
///      就包含「上市」及「上櫃」, 不能分別訂閱(或必須提供額外方式支援)。
/// 2. 上市上櫃分開資料表: 建立2個 TwsMdSys, 例: TwseMd 及 TpexMd;
///    - 在更新商品資料時, 必須將資料表鎖定,
///      上市上櫃分開, 可以減少搶鎖的情況發生。
///    - 使用者訂閱時, 需指定上市或上櫃, 例:
///      - 資料表 "/TwseMd/Symbs"; 商品 2330;
///      - 資料表 "/TpexMd/Symbs"; 商品 5201;
///    - 使用者若有訂閱整棵樹的權限,
///      則必須分別訂閱「上市」或「上櫃」。
///
/// Sapling 應該包含:
/// - Symbs, Indices: 建構時自動加入;
/// - 包含 MdReceiver 的 FpSession: 可以放在別處, 在建立 IoMgr 時提供即可;
/// - IoMgr: 由建構 ExgMdSystem 的人自行加入;
///   在 this->OnMdSystemStartup() 事件裡面, 會觸發 ioMgr->DisposeAndReopen();
class f9tws_API ExgMdSystem : public ExgMdSystemBase {
   fon9_NON_COPY_NON_MOVE(ExgMdSystem);
   using base = ExgMdSystemBase;
   fon9::AsyncFileAppenderSP  BaseInfoPkLog_;
   bool                       IsReloading_{false};
   char                       Padding____[7];

   void OnMdClearTimeChanged() override;
   void OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath) override;
   void OnParentTreeClear(fon9::seed::Tree& parent);

   ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, std::string rtiNamePre, bool isAddMarketSeq);

public:
   const ExgMdSymbsSP   Symbs_;
   const ExgMdSymbsSP   SymbsOdd_;
   const ExgMdIndicesSP Indices_;

   ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, bool useRtiForRecover, bool isAddMarketSeq);
   ~ExgMdSystem();

   /// 可以存任意交易所格式, 並於程式重新啟動時載入,
   /// 但通常應該只存基本資料, 例如:
   /// - 格式  1：個股基本資料.
   /// - 格式 14：認購(售)權證全稱資料.
   /// - 格式 15：當日停止交易資料.
   /// - 格式 21︰指數基本資料.
   /// - 格式 22︰盤中零股交易個股基本資料.
   void BaseInfoPkLog(const ExgMdHead& pk, unsigned pksz) {
      if (this->BaseInfoPkLog_ && !this->IsReloading_)
         this->BaseInfoPkLog_->Append(&pk, pksz);
   }
   bool IsReloading() const {
      return this->IsReloading_;
   }
};
using ExgMdSystemSP = fon9::intrusive_ptr<ExgMdSystem>;

static inline ExgMdSystem& TwsMdSys(const ExgMdHandler& handler) {
   assert(dynamic_cast<ExgMdSystem*>(&handler.PkSys_) != nullptr);
   return *static_cast<ExgMdSystem*>(&handler.PkSys_);
}

} // namespaces
#endif//__f9tws_ExgMdSystem_hpp__
