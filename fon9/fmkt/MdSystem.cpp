// \file fon9/fmkt/MdSystem.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdSystem.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/TimedFileName.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace fmkt {

MdSystem::MdSystem(seed::MaTreeSP root, std::string name)
   : base{name}
   , Root_{std::move(root)} {
}
MdSystem::~MdSystem() {
   this->ClearTimer_.DisposeAndWait();
}
void MdSystem::OnParentTreeClear(seed::Tree& parent) {
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
   (void)logPath;
   assert(tdayYYYYMMDD != this->TDayYYYYMMDD_);
   this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   this->SetDescription(RevPrintTo<std::string>("tday=", tdayYYYYMMDD));
   fon9_LOG_IMP("MdSystem.Startup|name=", this->Name_, '|', this->GetDescription());
}

} } // namespaces
