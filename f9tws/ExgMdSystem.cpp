// \file f9tws/ExgMdSystem.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdSystem.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/FileReadAll.hpp"
#include "f9tws/ExgMdPkReceiver.hpp"

namespace f9tws {

ExgMdHandler::~ExgMdHandler() {
}
// ----------------------------------------
ExgMdHandlerAnySeq::~ExgMdHandlerAnySeq() {
}
void ExgMdHandlerAnySeq::DailyClear() {
}
// ----------------------------------------
ExgMdHandlerPkCont::~ExgMdHandlerPkCont() {
}
void ExgMdHandlerPkCont::DailyClear() {
   this->Clear();
}
void ExgMdHandlerPkCont::OnPkReceived(const ExgMdHeader& pk, unsigned pksz) {
   this->FeedPacket(&pk, pksz, pk.GetSeqNo());
}
void ExgMdHandlerPkCont::LogSeqGap(const ExgMdHeader& pk, SeqT seq, SeqT lostCount) {
   if (fon9_UNLIKELY(this->NextSeq_ == 0)) {
      this->NextSeq_ = 1;
      if ((lostCount = (seq - this->NextSeq_)) == 0)
         return;
      this->LostCount_ += lostCount;
   }
   constexpr auto lv = fon9::LogLevel::Warn;
   if (fon9_UNLIKELY(lv >= fon9::LogLevel_)) {
      fon9::RevBufferList rbuf_{fon9::kLogBlockNodeSize};
      fon9::RevPutChar(rbuf_, '\n');
      if (lostCount > 1)
         fon9::RevPrint(rbuf_, "..", seq - 1);
      fon9::RevPrint(rbuf_,
                     "TwsExgMd.Gap|mkt=", pk.GetMarket(),
                     "|fmt=", pk.GetFmtNo(),
                     "|lost=", lostCount, ':', this->NextSeq_);
      fon9::LogWrite(lv, std::move(rbuf_));
   }
}
//--------------------------------------------------------------------------//
ExgMdSystem::ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, std::string rtiNamePre)
   : base(root, name)
   , Symbs_{new ExgMdSymbs{rtiNamePre.empty() ? std::string{} : (rtiNamePre + "Symbs")}}
   , SymbsOdd_{new ExgMdSymbs{rtiNamePre.empty() ? std::string{} : (rtiNamePre + "SymbsOdd")}}
   , Indices_{new ExgMdIndices{rtiNamePre.empty() ? std::string{} : (rtiNamePre + "Indices")}} {
   this->Sapling_->AddNamedSapling(this->Symbs_, "Symbs");
   this->Sapling_->AddNamedSapling(this->SymbsOdd_, "SymbsOdd");
   this->Sapling_->AddNamedSapling(this->Indices_, "Indices");
   this->Symbs_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->SymbsOdd_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->Indices_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
}
ExgMdSystem::ExgMdSystem(fon9::seed::MaTreeSP root, std::string name, bool useRtiForRecover)
   : ExgMdSystem(root, name, useRtiForRecover ? (fon9::seed::SysEnv_GetLogFileFmtPath(*root) + name + "_") : std::string{}) {
}
ExgMdSystem::~ExgMdSystem() {
}
void ExgMdSystem::OnParentTreeClear(fon9::seed::Tree& parent) {
   std::string logpath = this->LogPath() + "_";
   this->Symbs_->SaveTo(logpath + "Symbs.mds");
   this->SymbsOdd_->SaveTo(logpath + "SymbsOdd.mds");
   this->Indices_->SaveTo(logpath + "Indices.mds");
   base::OnParentTreeClear(parent);
}
void ExgMdSystem::OnMdClearTimeChanged() {
   this->Symbs_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->SymbsOdd_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
   this->Indices_->SetDailyClearHHMMSS(this->GetClearHHMMSS());
}
void ExgMdSystem::OnMdSystemStartup(const unsigned tdayYYYYMMDD, const std::string& logPath) {
   std::string fname = logPath + this->Name_;
   *(this->LogPath_.Lock()) = fname;

   this->BaseInfoPkLog_.reset();
   auto pkLogF = fon9::AsyncFileAppender::Make();
   fname.append("_BaseInfo.bin");
   auto resPkLog = pkLogF->OpenImmediately(
      fname, fon9::FileMode::CreatePath | fon9::FileMode::Append | fon9::FileMode::Read);
   if (resPkLog.IsError()) {
      pkLogF.reset();
      fon9_LOG_ERROR("Tws.ExgMdSystem.PkLog|fname=", fname, '|', resPkLog);
   }

   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
   this->Symbs_->DailyClear(tdayYYYYMMDD);
   this->SymbsOdd_->DailyClear(tdayYYYYMMDD);
   this->Indices_->DailyClear(tdayYYYYMMDD);

   auto seeds = this->Sapling_->GetList(nullptr);
   for (fon9::seed::NamedSeedSP& seed : seeds) {
      if (auto* ioMgr = dynamic_cast<fon9::IoManagerTree*>(seed->GetSapling().get()))
         ioMgr->DisposeAndReopen("Tws.OnMdSystemStartup", fon9::TimeInterval_Second(1));
   }

   for (const ExgMdHandlerSP& pph : this->MdHandlers_) {
      if (pph.get())
         pph->DailyClear();
   }
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
         this->Owner_.OnPkReceived(*static_cast<const ExgMdHeader*>(pkptr), pksz);
         return true;
      }
   };
   if (pkLogF) {
      BaseInfoReloader     reloader{*this};
      fon9::File::SizeType fpos = 0;
      resPkLog = fon9::FileReadAll(*pkLogF, fpos, reloader);
      if (resPkLog.IsError())
         fon9_LOG_ERROR("Tws.ExgMdSystem.PkLog.Read|fname=", fname, '|', resPkLog);
      this->BaseInfoPkLog_ = std::move(pkLogF);
   }
   // ----------------------------------------
   std::string logpath = this->LogPath() + "_";
   this->Symbs_->LoadFrom(logpath + "Symbs.mds");
   this->SymbsOdd_->LoadFrom(logpath + "Symbs.mds");
   this->Indices_->LoadFrom(logpath + "Indices.mds");
}

} // namespaces
