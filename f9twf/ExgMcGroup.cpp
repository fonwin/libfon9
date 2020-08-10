// \file f9twf/ExgMcGroup.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMcGroup.hpp"
#include "fon9/seed/SysEnv.hpp"

namespace f9twf {

ExgMcSystem::ExgMcSystem(fon9::seed::MaTreeSP root, std::string name, bool useRtiForRecover, bool isAddMarketSeq)
   : base(root, name)
   , Symbs_{new ExgMdSymbs(useRtiForRecover ? (fon9::seed::SysEnv_GetLogFileFmtPath(*root) + name) : std::string{}, isAddMarketSeq)} {
   this->Sapling_->AddNamedSapling(this->Symbs_, "Symbs");
   this->Symbs_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
}
ExgMcSystem::~ExgMcSystem() {
}
void ExgMcSystem::OnParentTreeClear(fon9::seed::Tree& parent) {
   this->Symbs_->SaveTo(this->LogPath_ + "_Symbs.mds");
   base::OnParentTreeClear(parent);
}
void ExgMcSystem::OnMdClearTimeChanged() {
   this->Symbs_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
}
void ExgMcSystem::OnMdSystemStartup(const unsigned tdayYYYYMMDD, const std::string& logPath) {
   this->LogPath_ = logPath + this->Name_;
   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
   this->Symbs_->DailyClear(tdayYYYYMMDD);

   auto seeds = this->Sapling_->GetList(nullptr);
   // 必須先啟動[夜盤], 因為一旦商品進入夜盤, 則不應再處理日盤訊息.
   // - 啟動時會先從「基本資料 Channel 保留檔」載入商品基本資料.
   //   所以先啟動夜盤, 則可以讓商品直接進入夜盤時段.
   for (fon9::seed::NamedSeedSP& seed : seeds) {
      if (auto* mcGroup = dynamic_cast<ExgMcGroup*>(seed.get()))
         if (mcGroup->ChannelMgr_->TradingSessionId_ == f9fmkt_TradingSessionId_AfterHour)
            mcGroup->StartupMcGroup(*this, logPath);
   }
   for (fon9::seed::NamedSeedSP& seed : seeds) {
      if (auto* mcGroup = dynamic_cast<ExgMcGroup*>(seed.get()))
         if (mcGroup->ChannelMgr_->TradingSessionId_ != f9fmkt_TradingSessionId_AfterHour)
            mcGroup->StartupMcGroup(*this, logPath);
   }
   this->Symbs_->LoadFrom(this->LogPath_ + "_Symbs.mds");
}
//--------------------------------------------------------------------------//
ExgMcGroup::ExgMcGroup(ExgMcSystem* mdsys, std::string name, f9fmkt_TradingSessionId tsesId)
   : base(std::move(name))
   , ChannelMgr_{new ExgMcChannelMgr(mdsys->Symbs_, &mdsys->Name_, &Name_, tsesId)} {
}
ExgMcGroup::~ExgMcGroup() {
}
void ExgMcGroup::StartupMcGroup(ExgMcSystem& mdsys, std::string logPath) {
   (void)mdsys;
   // logPath = "logs/yyyymmdd/TwfMd_MdDay_"
   logPath += this->ChannelMgr_->Name_ + "_";
   // 在設定 Channel 時, PkLog 的檔名為 sysLogPath + "NNNN.tsbin"; 其中 NNNN = ChannelId;
   this->ChannelMgr_->StartupChannelMgr(logPath);

   // IoMgr 必須在 StartupChannelMgr() 之後才啟動. 避免 Channel 還沒準備好, 但 IoMgr 已收到封包.
   auto seeds = this->Sapling_->GetList(nullptr);
   for (fon9::seed::NamedSeedSP& seed : seeds) {
      auto* handler = dynamic_cast<ExgMcGroupSetupHandler*>(seed.get());
      if (handler)
         handler->OnStartupMcGroup(*this, logPath);
      if ((handler = dynamic_cast<ExgMcGroupSetupHandler*>(seed->GetSapling().get())) != nullptr)
         handler->OnStartupMcGroup(*this, logPath);
   }
}
//--------------------------------------------------------------------------//
ExgMcGroupSetupHandler::~ExgMcGroupSetupHandler() {
}
ExgMcGroupIoMgr::~ExgMcGroupIoMgr() {
}
void ExgMcGroupIoMgr::OnStartupMcGroup(ExgMcGroup&, const std::string& logPath) {
   (void)logPath;
   this->DisposeAndReopen("StartupMcGroup", fon9::TimeInterval_Second(1));
}

} // namespaces
