// \file f9tws/ExgMdSystem.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdSystem.hpp"
#include "f9tws/ExgMdPkReceiver.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/FileReadAll.hpp"
#include "fon9/Log.hpp"

namespace f9tws {

ExgMdSystem::ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, std::string rtiNamePre, bool isAddMarketSeq)
   : base(root, name)
   , Symbs_{new ExgMdSymbs(rtiNamePre.empty() ? std::string{} : (rtiNamePre + "Symbs"), isAddMarketSeq)}
   , SymbsOdd_{new ExgMdSymbs(rtiNamePre.empty() ? std::string{} : (rtiNamePre + "SymbsOdd"), isAddMarketSeq)}
   , Indices_{new ExgMdIndices(rtiNamePre.empty() ? std::string{} : (rtiNamePre + "Indices"), isAddMarketSeq)} {
   this->Sapling_->AddNamedSapling(this->Symbs_, "Symbs");
   this->Sapling_->AddNamedSapling(this->SymbsOdd_, "SymbsOdd");
   this->Sapling_->AddNamedSapling(this->Indices_, "Indices");
   this->Symbs_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->SymbsOdd_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->Indices_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
}
ExgMdSystem::ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, bool useRtiForRecover, bool isAddMarketSeq)
   : ExgMdSystem(root, name,
                 useRtiForRecover ? (fon9::seed::SysEnv_GetLogFileFmtPath(*root) + name + "_") : std::string{},
                 isAddMarketSeq) {
}
ExgMdSystem::~ExgMdSystem() {
}
void ExgMdSystem::OnParentTreeClear(fon9::seed::Tree& parent) {
   std::string logpath = this->LogPathPre();
   this->Symbs_   ->SaveTo(logpath + "_Symbs.mds");
   this->SymbsOdd_->SaveTo(logpath + "_SymbsOdd.mds");
   this->Indices_ ->SaveTo(logpath + "_Indices.mds");
   base::OnParentTreeClear(parent);
}
void ExgMdSystem::OnMdClearTimeChanged() {
   this->Symbs_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->SymbsOdd_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->Indices_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
}
void ExgMdSystem::OnMdSystemStartup(const unsigned tdayYYYYMMDD, const std::string& logPath) {
   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
   const std::string fnamePre = this->LogPathPre();
   this->BaseInfoPkLog_.reset();
   auto pkLogF = fon9::AsyncFileAppender::Make();
   const std::string fnamePkLog = fnamePre + "_BaseInfo.bin";
   auto resPkLog = pkLogF->OpenImmediately(
      fnamePkLog, fon9::FileMode::CreatePath | fon9::FileMode::Append | fon9::FileMode::Read);
   if (resPkLog.IsError()) {
      pkLogF.reset();
      fon9_LOG_ERROR("Tws.ExgMdSystem.PkLog|fname=", fnamePkLog, '|', resPkLog);
   }
   this->Symbs_   ->DailyClear(tdayYYYYMMDD);
   this->SymbsOdd_->DailyClear(tdayYYYYMMDD);
   this->Indices_ ->DailyClear(tdayYYYYMMDD);
   // ----------------------------------------
   struct BaseInfoReloader : public ExgMdPkReceiver {
      fon9_NON_COPY_NON_MOVE(BaseInfoReloader);
      ExgMdSystem&   Owner_;
      BaseInfoReloader(ExgMdSystem& owner) : Owner_(owner) {
         owner.IsReloading_ = true;
      }
      ~BaseInfoReloader() {
         this->Owner_.IsReloading_ = false;
      }
      bool operator()(fon9::DcQueue& rdbuf, fon9::File::Result&) {
         this->FeedBuffer(rdbuf);
         return true;
      }
      bool OnPkReceived(const void* pkptr, unsigned pksz) override {
         this->Owner_.OnPkReceived(*static_cast<const ExgMdHead*>(pkptr), pksz);
         return true;
      }
   };
   if (pkLogF) {
      BaseInfoReloader     reloader{*this};
      fon9::File::SizeType fpos = 0;
      resPkLog = fon9::FileReadAll(*pkLogF, fpos, reloader);
      if (resPkLog.IsError())
         fon9_LOG_ERROR("Tws.ExgMdSystem.PkLog.Read|fname=", fnamePkLog, '|', resPkLog);
      this->BaseInfoPkLog_ = std::move(pkLogF);
   }
   // ----------------------------------------
   this->Symbs_   ->LoadFrom(fnamePre + "_Symbs.mds");
   this->SymbsOdd_->LoadFrom(fnamePre + "_SymbsOdd.mds");
   this->Indices_ ->LoadFrom(fnamePre + "_Indices.mds");
}

} // namespaces
