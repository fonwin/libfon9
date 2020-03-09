// \file f9twf/ExgMcGroup.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcGroup_hpp__
#define __f9twf_ExgMcGroup_hpp__
#include "f9twf/ExgMcChannel.hpp"
#include "fon9/framework/IoManagerTree.hpp"

namespace f9twf {

/// Sapling 包含:
/// - FpSession: McReceiver, MrRecover, MiSender...
/// - Symbs
/// - 根據設定加入 McGroup.
class f9twf_API ExgMcSystem : public fon9::seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(ExgMcSystem);
   using base = fon9::seed::NamedMaTree;

   unsigned TDayYYYYMMDD_{};
   unsigned ClearHHMMSS_{60000};

   static void EmitOnClearTimer(fon9::TimerEntry* timer, fon9::TimeStamp now);
   using ClearTimer = fon9::DataMemberEmitOnTimer<&ExgMcSystem::EmitOnClearTimer>;
   ClearTimer ClearTimer_;

   /// 考慮清檔時間 this->ClearHHMMSS_, 例:
   /// 若清檔時間為 06:00; 則在 [00:00..06:00) 之間啟動, 應視為前一日.
   unsigned CheckTDayYYYYMMDD(fon9::TimeStamp tm) const;

   bool Startup(unsigned tdayYYYYMMDD);

   void OnParentTreeClear(fon9::seed::Tree& parent) override;

public:
   const ExgMdSymbsSP         Symbs_;
   /// 用 Root_ 取得系統參數, 例如: SysEnv_GetLogFileFmtPath();
   const fon9::seed::MaTreeSP Root_;

   ExgMcSystem(fon9::seed::MaTreeSP root, std::string name);
   ~ExgMcSystem();

   unsigned GetTDayYYYYMMDD() const {
      return this->TDayYYYYMMDD_;
   }
   unsigned GetClearHHMMSS() const {
      return this->ClearHHMMSS_;
   }
   /// 設定清檔(檔案換日)時間.
   void SetClearHHMMSS(unsigned clearHHMMSS);

   /// 啟動(or 換日清檔).
   /// 已根據設定建立好相關物件後, 呼叫此處啟動.
   void StartupMcSystem();
};
using ExgMcSystemSP = fon9::intrusive_ptr<ExgMcSystem>;

//--------------------------------------------------------------------------//
/// - 建構後 Sapling 為空, 但可視情況, 自行加入底下物件.
///   - "ChannelMgr" = ExgMcChannelMgr
///   - "IoMgr"      = ExgMcGroupIoMgr
///   - "ToMiConvF"  = ExgMcToMiConv
///   - "ToMiConvO"  = ExgMcToMiConv
class f9twf_API ExgMcGroup : public fon9::seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(ExgMcGroup);
   using base = fon9::seed::NamedMaTree;
public:
   const ExgMcChannelMgrSP ChannelMgr_;

   ExgMcGroup(ExgMcSystem* mdsys, std::string name, f9fmkt_TradingSessionId tsesId);
   ~ExgMcGroup();

   /// 啟動(or 換日清檔), 從 ExgMcSystem::StartupMcSystem() 呼叫到此.
   /// 此時 logPath = "logs/yyyymmdd/";
   void StartupMcGroup(ExgMcSystem& mdsys, std::string logPath);
};
using ExgMcGroupSP = fon9::intrusive_ptr<ExgMcGroup>;

/// 在 ExgMcGroup::StartupMcGroup() 時呼叫 OnStartupMcGroup().
/// - 本身必須是加入 ExgMcGroup 的 NamedSeed, 或 NamedSeed->Sapling();
/// - 請參考 ExgMcGroupIoMgr 的做法.
class f9twf_API ExgMcGroupSetupHandler {
public:
   virtual ~ExgMcGroupSetupHandler();

   /// logPath 範例 "logs/yyyymmdd/TwfMd_MdDay_"
   virtual void OnStartupMcGroup(ExgMcGroup&, const std::string& logPath) = 0;
};

//--------------------------------------------------------------------------//
class f9twf_API ExgMcGroupIoMgr : public fon9::IoManagerTree, public ExgMcGroupSetupHandler {
   fon9_NON_COPY_NON_MOVE(ExgMcGroupIoMgr);
   using base = fon9::IoManagerTree;
public:
   const ExgMcGroupSP   McGroup_;

   ExgMcGroupIoMgr(const fon9::IoManagerArgs& args, ExgMcGroupSP owner)
      : base{args, fon9::TimeInterval::Null()}
      , McGroup_{std::move(owner)} {
   }
   ~ExgMcGroupIoMgr();

   void OnStartupMcGroup(ExgMcGroup&, const std::string& logPath) override;
};

} // namespaces
#endif//__f9twf_ExgMcGroup_hpp__
