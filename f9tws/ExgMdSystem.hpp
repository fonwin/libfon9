// \file f9tws/ExgMdSystem.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdSystem_hpp__
#define __f9tws_ExgMdSystem_hpp__
#include "fon9/fmkt/MdSystem.hpp"
#include "fon9/FileAppender.hpp"
#include "fon9/PkCont.hpp"
#include "f9tws/ExgMdSymbs.hpp"
#include "f9tws/ExgMdIndices.hpp"
#include "f9tws/ExgMdFmt.hpp"

namespace f9tws {

class f9tws_API ExgMdSystem;

struct f9tws_API ExgMdHandler {
   fon9_NON_COPY_NON_MOVE(ExgMdHandler);
   ExgMdSystem& MdSys_;
   ExgMdHandler(ExgMdSystem& mdsys) : MdSys_(mdsys) {
   }
   virtual ~ExgMdHandler();
   virtual void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) = 0;
   virtual void DailyClear() = 0;
};
using ExgMdHandlerSP = std::unique_ptr<ExgMdHandler>;

struct f9tws_API ExgMdHandlerAnySeq : public ExgMdHandler {
   fon9_NON_COPY_NON_MOVE(ExgMdHandlerAnySeq);
   using ExgMdHandler::ExgMdHandler;
   virtual ~ExgMdHandlerAnySeq();
   /// do nothing;
   void DailyClear() override;
};

class f9tws_API ExgMdHandlerPkCont : public ExgMdHandler, public fon9::PkContFeeder {
   fon9_NON_COPY_NON_MOVE(ExgMdHandlerPkCont);
   // fon9::PkContFeeder::FeedPacket(&pk, pksz, pk.GetSeqNo());
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
   // fon9::PkContFeeder::Clear();
   void DailyClear() override;
   void LogSeqGap(const ExgMdHeader& pk, SeqT seq, SeqT lostCount);
public:
   using ExgMdHandler::ExgMdHandler;
   virtual ~ExgMdHandlerPkCont();

   void CheckLogLost(const ExgMdHeader& pk, SeqT seq) {
      if (auto lostCount = (seq - this->NextSeq_))
         this->LogSeqGap(pk, seq, lostCount);
   }
};
//--------------------------------------------------------------------------//
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
class f9tws_API ExgMdSystem : public fon9::fmkt::MdSystem {
   fon9_NON_COPY_NON_MOVE(ExgMdSystem);
   using base = fon9::fmkt::MdSystem;
   fon9::MustLock<std::string>   LogPath_;
   using MdHandlers = ExgMdMessageDispatcher<ExgMdHandlerSP>;
   MdHandlers                 MdHandlers_;
   fon9::AsyncFileAppenderSP  BaseInfoPkLog_;
   bool                       IsReloading_{false};
   char                       Padding____[7];

   void OnMdClearTimeChanged() override;
   void OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath) override;
   void OnParentTreeClear(fon9::seed::Tree& parent);

   ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, std::string rtiNamePre);

public:
   const ExgMdSymbsSP   Symbs_;
   const ExgMdSymbsSP   SymbsOdd_;
   const ExgMdIndicesSP Indices_;

   ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, bool useRtiForRecover);
   ~ExgMdSystem();

   /// "./logs/yyyymmdd/" + this->Name_;
   /// this->Name_ 通常為 "TwsMd", "TwseMd", "TpexMd"... 之類;
   /// 尾端沒有 '/';
   std::string LogPath() const {
      return *this->LogPath_.ConstLock();
   }

   void RegMdMessageHandler(ExgMdMarket mkt, uint8_t fmt, uint8_t ver, ExgMdHandlerSP handler) {
      this->MdHandlers_.Reg(mkt, fmt, ver, std::move(handler));
   }
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) {
      if (ExgMdHandler* handler = this->MdHandlers_.Get(pk).get())
         handler->OnPkReceived(pk, pksz);
   }

   /// 可以存任意交易所格式, 並於程式重新啟動時載入,
   /// 但通常應該只存基本資料, 例如:
   /// - 格式  1：個股基本資料.
   /// - 格式 14：認購(售)權證全稱資料.
   /// - 格式 15：當日停止交易資料.
   /// - 格式 21︰指數基本資料.
   /// - 格式 22︰盤中零股交易個股基本資料.
   void BaseInfoPkLog(const ExgMdHeader& pk, unsigned pksz) {
      if (this->BaseInfoPkLog_ && !this->IsReloading_)
         this->BaseInfoPkLog_->Append(&pk, pksz);
   }
   bool IsReloading() const {
      return this->IsReloading_;
   }
};
using ExgMdSystemSP = fon9::intrusive_ptr<ExgMdSystem>;

} // namespaces
#endif//__f9tws_ExgMdSystem_hpp__
