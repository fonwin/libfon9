// \file fon9/fmkt/MdSystem.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdSystem_hpp__
#define __fon9_fmkt_MdSystem_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/SchTask.hpp"

namespace fon9 { namespace fmkt {

/// 行情接收系統共用基底.
/// - TDay 換日管理.
/// - 每日清檔計時, 預設為每日 06:00:00.
class fon9_API MdSystem : public seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(MdSystem);
   using base = seed::NamedMaTree;

   fon9::MustLock<std::string>   LogPathPre_;

   unsigned TDayYYYYMMDD_{};
   unsigned ClearHHMMSS_{60000};

   static void EmitOnClearTimer(TimerEntry* timer, TimeStamp now);
   using ClearTimer = DataMemberEmitOnTimer<&MdSystem::EmitOnClearTimer>;
   ClearTimer ClearTimer_;

   /// 考慮清檔時間 this->ClearHHMMSS_, 例:
   /// 若清檔時間為 06:00; 且系統在 [00:00..06:00) 之間啟動, 應視為前一日.
   unsigned CheckTDayYYYYMMDD(TimeStamp tm) const;

   void SetInfoToDescription();

protected:
   fon9::SchConfig   NoDataEventSch_;

   /// 系統重啟, 或換日啟動.
   /// - 繼承者必須先呼叫這裡, 否則 this->GetTDayYYYYMMDD() 會取得先前的 TDay;
   /// - 此處的 logPath 只包含路徑, 範例: "logs/yyyymmdd/"; 
   /// - 返回前設定 this->LogPathPre = logPath + this->Name_;
   /// - 重新開啟 this->Sapling_ 底下的 IoManagerTree.DisposeAndReopen();
   virtual void OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath);
   /// 設定好新的 ClearHHMMSS_ 之後, 先觸發此事件, 然後再檢查 TDay 是否可能有變.
   /// 這裡預設 do nothing.
   virtual void OnMdClearTimeChanged();

   /// 除了呼叫 base, 還有 this->ClearTimer_.DisposeAndWait();
   void OnParentTreeClear(seed::Tree& parent) override;

public:
   /// 用 Root_ 取得系統參數, 例如: SysEnv_GetLogFileFmtPath();
   const seed::MaTreeSP Root_;

   MdSystem(seed::MaTreeSP root, std::string name);
   ~MdSystem();

   unsigned GetTDayYYYYMMDD() const {
      return this->TDayYYYYMMDD_;
   }
   unsigned GetClearHHMMSS() const {
      return this->ClearHHMMSS_;
   }
   /// 設定清檔(檔案換日)時間.
   /// - 設定新的清檔時間, 會先觸發 OnMdClearTimeChanged();
   /// - 然後使用「現在時間」檢查 TDay 是否改變, 若有則會觸發 OnMdSystemStartup();
   void SetClearHHMMSS(unsigned clearHHMMSS);

   /// 設定有資料的時間範圍.
   /// 同時設定, 清盤時間: this->SetClearHHMMSS(this->NoDataEventSch_.StartTime_.ToHHMMSS());
   /// 在 this->NoDataEventSch_ 範圍內若無任何資料, 則應觸發 NoDataEvent (由衍生者實作)
   void SetNoDataEventSch(fon9::StrView cfgstr);

   /// 啟動(or 換日清檔).
   /// 已根據設定建立好相關物件後, 呼叫此處啟動.
   /// - 若 tday 有變動, 則會觸發 OnMdSystemStartup();
   /// - 記錄 log;
   /// - 重啟 ClearTimer_;
   void StartupMdSystem(TimeStamp nowLocalTime = LocalNow());

   /// "./logs/yyyymmdd/" + this->Name_;
   /// this->Name_ 通常為 "TwsMd", "TwseMd", "TpexMd"... 之類;
   /// 尾端沒有 '/';
   std::string LogPathPre() const {
      return *this->LogPathPre_.ConstLock();
   }
};
using MdSystemSP = intrusive_ptr<MdSystem>;

} } // namespaces
#endif//__fon9_fmkt_MdSystem_hpp__
