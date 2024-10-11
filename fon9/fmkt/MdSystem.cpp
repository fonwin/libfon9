// \file fon9/fmkt/MdSystem.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdSystem.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/TimedFileName.hpp"
#include "fon9/framework/IoManagerTree.hpp"

namespace fon9 { namespace fmkt {

MdSystem::MdSystem(seed::MaTreeSP root, std::string name)
   : base{name}
   , Root_{std::move(root)} {
}
MdSystem::~MdSystem() {
   this->ClearTimer_.DisposeAndWait();
}
void MdSystem::OnParentTreeClear(seed::Tree& parent) {
   auto seeds = this->Sapling_->GetList(nullptr);
   for (fon9::seed::NamedSeedSP& seed : seeds) {
      if (auto* ioMgr = dynamic_cast<IoManagerTree*>(seed->GetSapling().get()))
         ioMgr->DisposeDevices(this->Name_ + ".OnParentTreeClear");
   }
   base::OnParentTreeClear(parent);
   this->ClearTimer_.DisposeAndWait();
}
void MdSystem::EmitOnClearTimer(TimerEntry* timer, TimeStamp now) {
   (void)now;
   MdSystem& rthis = ContainerOf(*static_cast<ClearTimer*>(timer), &MdSystem::ClearTimer_);
   rthis.StartupMdSystem();
}
void MdSystem::OnMdClearTimeChanged() {
}
void MdSystem::SetClearHHMMSS(unsigned clearHHMMSS) {
   if (this->ClearHHMMSS_ == clearHHMMSS)
      return;
   this->ClearHHMMSS_ = clearHHMMSS;
   this->OnMdClearTimeChanged();
   if (this->TDayYYYYMMDD_) // 必須有啟動過, 才需要重啟計時器.
      this->StartupMdSystem();
}
void MdSystem::SetNoDataEventSch(StrView cfgstr) {
   this->NoDataEventSch_.Parse(cfgstr);
   this->SetClearHHMMSS(this->NoDataEventSch_.StartTime_.ToHHMMSS());
}
unsigned MdSystem::CheckTDayYYYYMMDD(TimeStamp tm) const {
   if (GetHHMMSS(tm) < this->ClearHHMMSS_)
      tm -= TimeInterval_Day(1);
   return GetYYYYMMDD(tm);
}
void MdSystem::StartupMdSystem(TimeStamp nowLocalTime) {
   unsigned tdayYYYYMMDD = this->CheckTDayYYYYMMDD(nowLocalTime);
   if (this->TDayYYYYMMDD_ != tdayYYYYMMDD) {
      TimedFileName logfn(seed::SysEnv_GetLogFileFmtPath(*this->Root_), TimedFileName::TimeScale::Day);
      logfn.RebuildFileNameYYYYMMDD(tdayYYYYMMDD); // logfn.GetFileName() = "logs/yyyymmdd/"
      this->OnMdSystemStartup(tdayYYYYMMDD, logfn.GetFileName());
      assert(this->TDayYYYYMMDD_ == tdayYYYYMMDD);
   }
   TimeStamp nextClearLocalTime = (YYYYMMDDHHMMSS_ToTimeStamp(this->TDayYYYYMMDD_, this->ClearHHMMSS_)
                                   + TimeInterval_Day(1));
   fon9_LOG_IMP("MdSystem.NextClear|name=", this->Name_, "|time=", nextClearLocalTime);
   this->ClearTimer_.RunAt(nextClearLocalTime - GetLocalTimeZoneOffset());
}
void MdSystem::OnMdSystemStartup(const unsigned tdayYYYYMMDD, const std::string& logPath) {
   assert(tdayYYYYMMDD != this->TDayYYYYMMDD_);
   this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   *(this->LogPathPre_.Lock()) = logPath + this->Name_;
   auto seeds = this->Sapling_->GetList(nullptr);
   for (fon9::seed::NamedSeedSP& seed : seeds) {
      if (auto* ioMgr = dynamic_cast<IoManagerTree*>(seed->GetSapling().get()))
         ioMgr->DisposeAndReopen(this->Name_ + ".OnMdSystemStartup", TimeInterval_Second(1));
   }
   this->SetInfoToDescription();
}
void MdSystem::SetInfoToDescription() {
   RevBufferFixedSize<1024> rbuf;
   if (!this->NoDataEventSch_.IsAlwaysInSch())
      RevPrint(rbuf, "|DataInSch='", this->NoDataEventSch_, "'");
   RevPrint(rbuf, "TDay=", this->TDayYYYYMMDD_);
   this->SetDescription(rbuf.ToStrT<std::string>());
   fon9_LOG_IMP("MdSystem.Startup|name=", this->Name_, '|', this->GetDescription());
}
//--------------------------------------------------------------------------//
const TimeInterval   MdSystemWithHb::kMdStChkInterval = TimeInterval_Second(5);
const TimeInterval   MdSystemWithHb::kMdHbInterval    = TimeInterval_Second(35);

MdSystemWithHb::~MdSystemWithHb() {
   this->HbTimer_.DisposeAndWait();
}
TimeInterval MdSystemWithHb::OnMdSystem_HbTimer(TimeStamp now) {
   (void)now;
   // 會來到這裡, 表示衍生者沒有處理 OnMdSystem_HbTimer(), 所以停止 HbTimer_; 不再浪費系統資源.
   this->HbTimer_.StopNoWait();
   return TimeInterval{};
}
void MdSystemWithHb::OnParentTreeClear(fon9::seed::Tree& parent) {
   base::OnParentTreeClear(parent);
   this->HbTimer_.DisposeAndWait();
}
void MdSystemWithHb::OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath) {
   this->HbTimer_.RunAfter(kMdStChkInterval);
   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
}
void MdSystemWithHb::EmitHbTimer(TimerEntry* timer, TimeStamp now) {
   MdSystemWithHb& rthis = ContainerOf(*static_cast<decltype(MdSystemWithHb::HbTimer_)*>(timer), &MdSystemWithHb::HbTimer_);
   const auto      sch = rthis.NoDataEventSch_.Check(now, SchSt::Unknown);
   if (sch.SchSt_ == SchSt::InSch)
      timer->RunAfter(rthis.OnMdSystem_HbTimer(now));
   else
      timer->RunAfter(kMdHbInterval);
}

} } // namespaces
