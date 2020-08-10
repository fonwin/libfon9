// \file f9twf/ExgMcGroup.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcGroup_hpp__
#define __f9twf_ExgMcGroup_hpp__
#include "f9twf/ExgMcChannel.hpp"
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/fmkt/MdSystem.hpp"

namespace f9twf {

/// Sapling 應該包含:
/// - Symbs (建構時自動加入)
/// - FpSession: McReceiver, MrRecover, MiSender...
/// - 根據設定加入 McGroup.
class f9twf_API ExgMcSystem : public fon9::fmkt::MdSystem {
   fon9_NON_COPY_NON_MOVE(ExgMcSystem);
   using base = fon9::fmkt::MdSystem;
   std::string LogPath_;

protected:
   void OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath) override;
   void OnMdClearTimeChanged() override;
   void OnParentTreeClear(fon9::seed::Tree& parent);

public:
   const ExgMdSymbsSP   Symbs_;

   ExgMcSystem(fon9::seed::MaTreeSP root, std::string name, bool useRtiForRecover, bool isAddMarketSeq);
   ~ExgMcSystem();
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
