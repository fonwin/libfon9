// \file fon9/seed/FileImpTree.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/FileImpTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include <sys/stat.h>

namespace fon9 {
namespace seed {

FileImpMgr::~FileImpMgr() {
   this->MonitorTimer_.DisposeAndWait();
   this->DisposeAndWait();
}
TreeSP FileImpMgr::GetSapling() {
   return this->Sapling_;
}
StrView FileImpMgr::Name() const {
   return &this->Name_;
}
void FileImpMgr::OnSeedCommand(SeedOpResult&          res,
                               StrView                cmdln,
                               FnCommandResultHandler resHandler,
                               ConfigLocker&&         ulk) {
   (void)ulk; assert(!ulk.owns_lock());
   res.OpResult_ = OpResult::no_error;
   fon9::StrTrim(&cmdln);
   if (cmdln == "reload") {
      this->LoadAll();
      resHandler(res, "Reload all is done.");
      return;
   }
   if (cmdln == "?") {
      resHandler(res, "reload"
                 fon9_kCSTR_CELLSPL "Reload all."
                 fon9_kCSTR_CELLSPL "[cause for log]");
      return;
   }
   res.OpResult_ = OpResult::not_supported_cmd;
   resHandler(res, cmdln);
}
void FileImpMgr::LoadAll() {
   this->MonitorTimer_.StopAndWait();
   fon9_LOG_INFO("FileImpMgr|Info=LoadAll.Start"
                 "|Name=", this->OwnerTree_.ConfigMgr_.Name(), '/', this->Name_);
   static_cast<FileImpTree*>(this->Sapling_.get())->LoadAll();
   this->SetNextTimeInfo(this->GetNextSchInTime(), "LoadAll.Done");
   this->MonitorTimer_.RunAfter(TimeInterval_Second(1));
}
void FileImpMgr::OnSchTask_StateChanged(bool isInSch) {
   if (isInSch) {
      intrusive_ptr<FileImpMgr> pthis{this};
      GetDefaultThreadPool().EmplaceMessage(std::bind(&FileImpMgr::LoadAll, pthis));
   }
}
void FileImpMgr::OnSchCfgChanged(StrView cfgstr) {
   if (this->Start(cfgstr) == SchSt::Unknown) // 初次啟動時載入.
      this->LoadAll();
}
void FileImpMgr::CheckStartMonitor(FileImpTree::Locker&& lk) {
   lk.unlock();
   this->MonitorTimer_.RunAfter(TimeInterval_Second(1));
}
void FileImpMgr::OnMonitorTimer(TimerEntry* timer, fon9::TimeStamp now) {
   FileImpMgr& rthis = ContainerOf(*static_cast<decltype(FileImpMgr::MonitorTimer_)*>(timer), &FileImpMgr::MonitorTimer_);
   {
      SchImpl::Locker schImpl{rthis.SchImpl_};
      if (schImpl->SchSt_ != SchSt::InSch)
         return;
   }
   TimeInterval tiAfter;
   {
      FileImpTree::Locker  imps{rthis.GetFileImpSapling().Lock()};
      for (size_t idx = 0; idx < imps->size(); ++idx) {
         NamedSeedSP seed = imps->sindex(idx);
         if (FileImpSeed* imp = dynamic_cast<FileImpSeed*>(seed.get())) {
            if (imp->GetMonitorFlag(imps) == FileImpMonitorFlag::None
                && !imp->IsForceLoadOnce())
               continue;
            TimeInterval ti = imp->MonitorCheck(std::move(imps), now);
            if (!ti.IsNullOrZero()) {
               if (tiAfter.IsNullOrZero() || tiAfter > ti)
                  tiAfter = ti;
            }
            if (!imps.owns_lock())
               imps.lock();
         }
      }
   }
   if (!tiAfter.IsNullOrZero())
      timer->RunAfter(tiAfter);
}
void FileImpMgr::ClearReloadAll() {
   FileImpTree::Locker  imps{this->GetFileImpSapling().Lock()};
   for (size_t idx = 0; idx < imps->size(); ++idx) {
      if (FileImpSeed* imp = dynamic_cast<FileImpSeed*>(imps->sindex(idx).get())) {
         imp->ClearReload(std::move(imps));
         if (!imps.owns_lock())
            imps.lock();
      }
   }
}
//--------------------------------------------------------------------------//
void FileImpTree::LoadAll() {
   Locker imps{this->Container_};
   for (size_t idx = 0; idx < imps->size(); ++idx) {
      NamedSeedSP seed = imps->sindex(idx);
      if (FileImpSeed* imp = dynamic_cast<FileImpSeed*>(seed.get())) {
         imps.unlock();
         imp->LoadFrom(std::move(imps), nullptr);
         assert(imps.owns_lock());
      }
   }
}
//--------------------------------------------------------------------------//
FileImpSeed::~FileImpSeed() {
}
LayoutSP FileImpSeed::MakeLayout(TabFlag tabFlag) {
   Fields fields;
   fields.Add(fon9_MakeField      (FileImpSeed, Title_,       "FileName"));
   fields.Add(fon9_MakeField      (FileImpSeed, Value_,       "Mon", "Monitor", "Auto reload when FileTime changed."));
   fields.Add(fon9_MakeField      (FileImpSeed, SchCfgStr_,   "MonSch"));
   fields.Add(fon9_MakeField_const(FileImpSeed, Description_, "Result"));
   return new Layout1(fon9_MakeField2(FileImpSeed, Name),
                      new Tab(Named{"imp"}, std::move(fields), tabFlag));
}
void FileImpSeed::OnSeedCommand(SeedOpResult&          res,
                                StrView                cmdln,
                                FnCommandResultHandler resHandler,
                                ConfigLocker&&         ulk) {
   StrView cmd = StrFetchTrim(cmdln, &isspace);
   StrTrim(&cmdln);
   res.OpResult_ = OpResult::no_error;
   if (cmd == "reload") {
      // reload [fileName]
      this->LoadFrom(std::move(ulk), cmdln);
      assert(ulk.owns_lock());
      resHandler(res, &this->GetDescription());
      return;
   }
   if (cmd == "?") {
      resHandler(res, "reload"
                 fon9_kCSTR_CELLSPL "Reload data file."
                 fon9_kCSTR_CELLSPL "[filename]");
      return;
   }
   res.OpResult_ = OpResult::not_supported_cmd;
   resHandler(res, cmd);
}

void FileImpSeed::OnConfigValueChanged(ConfigLocker&& lk, StrView val) {
   char chMonFlag = static_cast<char>(fon9::StrTrimHead(&val).Get1st());
   switch (static_cast<FileImpMonitorFlag>(chMonFlag)) {
   case FileImpMonitorFlag::AddTail:
   case FileImpMonitorFlag::Reload:
      val.Reset(&chMonFlag, &chMonFlag + 1);
      break;
   case FileImpMonitorFlag::None:
   default:
      val.Reset(nullptr);
      break;
   }
   base::OnConfigValueChanged(std::move(lk), val);
   assert(lk.owns_lock());
   static_cast<FileImpMgr*>(&this->OwnerTree_.ConfigMgr_)->CheckStartMonitor(std::move(lk));
}
TimeInterval FileImpSeed::MonitorCheck(ConfigLocker&& lk, TimeStamp now) {
   assert(lk.owns_lock());
   if (this->IsReloading_)
      return TimeInterval_Second(1);
   if (this->IsForceLoadOnce_) {
      this->Reload(std::move(lk), this->GetTitle(), FileImpMonitorFlag::Reload);
      return TimeInterval_Millisecond(1);
   }
   FileImpMonitorFlag monFlag = this->GetMonitorFlag(lk);
   switch (monFlag) {
   case FileImpMonitorFlag::None:
      return TimeInterval{};
   case FileImpMonitorFlag::AddTail:
   case FileImpMonitorFlag::Reload:
      SchConfig schcfg{ToStrView(this->SchCfgStr_)};
      auto      schres = schcfg.Check(now, SchSt::Unknown);
      if (schres.SchSt_ != SchSt::InSch)
         return TimeInterval_Second(1);

   #define kTimeInterval_FileNotChanged   TimeInterval_Millisecond(200)
   #ifdef fon9_WINDOWS
      WIN32_FILE_ATTRIBUTE_DATA fad;
      if (!::GetFileAttributesEx(this->GetTitle().c_str(), GetFileExInfoStandard, &fad)) {
         // GetLastError();
         break;
      }
      if (monFlag == FileImpMonitorFlag::AddTail) {
         ULARGE_INTEGER fsize;
         fsize.HighPart = fad.nFileSizeHigh;
         fsize.LowPart  = fad.nFileSizeLow;
         if (fsize.QuadPart <= this->LastPos_)
            return kTimeInterval_FileNotChanged;
      }
      else {
         assert(monFlag == FileImpMonitorFlag::Reload);
         if (this->LastFileTime_ == ToTimeStamp(fad.ftLastWriteTime))
            return kTimeInterval_FileNotChanged;
      }
   #else
      struct stat fst;
      if (stat(this->GetTitle().c_str(), &fst) != 0) {
         // errno;
         break;
      }
      if (monFlag == FileImpMonitorFlag::AddTail) {
         if (unsigned_cast(fst.st_size) <= this->LastPos_)
            return kTimeInterval_FileNotChanged;
      }
      else {
         assert(monFlag == FileImpMonitorFlag::Reload);
         if (this->LastFileTime_ == ToTimeStamp(fst.st_mtim))
            return kTimeInterval_FileNotChanged;
      }
   #endif
      this->Reload(std::move(lk), this->GetTitle(), monFlag);
      return TimeInterval_Millisecond(1);
   }
   // Check file st error.
   return TimeInterval_Second(1);
}
void FileImpSeed::LoadFrom(ConfigLocker&& ulk, StrView fname) {
   ulk.lock();
   std::string strfn;
   bool        isNeedsWriteConfig = false;
   if (fname.empty())
      strfn = this->GetTitle();
   else {
      strfn = fname.ToString();
      if (this->GetTitle().empty() || fname != &this->GetTitle()) {
         this->SetTitle(strfn);
         isNeedsWriteConfig = true;
      }
   }
   this->Reload(std::move(ulk), std::move(strfn), FileImpMonitorFlag::Reload);
   assert(ulk.owns_lock());
   if (isNeedsWriteConfig)
      this->OwnerTree_.WriteConfig(ulk, this);
}
void FileImpSeed::Reload(ConfigLocker&& lk, std::string fname, FileImpMonitorFlag monFlag) {
   this->IsForceLoadOnce_ = false;
   assert(lk.owns_lock());
   if (this->IsReloading_) {
      this->SetDescription("Loading");
      return;
   }
   struct ExitAutoRun {
      FileImpSeed*    Owner_;
      FileImpLoaderSP Loader_;
      ExitAutoRun(FileImpSeed* owner) : Owner_(owner) {
         owner->IsReloading_ = true;
      }
      ~ExitAutoRun() {
         fon9_LOG_INFO("FileImpSeed.Done|Name=", this->Owner_->Name_,
                       "|loader=", ToPtr(this->Loader_.get()),
                       "|result=", this->Owner_->GetDescription());
         this->Owner_->IsReloading_ = false;
      }
   };
   ExitAutoRun exitAutoRun{this};
   lk.unlock();
   if (monFlag != FileImpMonitorFlag::AddTail) {
      this->LastPos_ = 0;
      this->LastRemain_.clear();
   }
   fon9_LOG_INFO("FileImpSeed.Start|Name=", this->Name_, "|fname=", fname, "|pos=", this->LastPos_);

   File    fd;
   StrView errfnc;
   auto    res = fd.Open(fname, FileMode::Read);
   RevBufferList rbuf{256};
   if (res.IsError()) {
      errfnc = "Open";
   __ERROR_FNC:;
      RevPrint(rbuf, '|', errfnc, ' ', res);
   __ADD_DESC_HEAD_INFO_AND_RETURN:;
      RevPrint(rbuf, LocalNow(), FmtTS{"f-T.6"}, "|fname=", fname);
      lk.lock();
      this->SetDescription(BufferTo<std::string>(rbuf.MoveOut()));
      this->OwnerTree_.UpdateConfigSeed(lk, *this, nullptr, nullptr);
      return;
   }
   res = fd.GetFileSize();
   if (res.IsError()) {
      errfnc = "GetFileSize";
      goto __ERROR_FNC;
   }
   this->LastFileTime_ = fd.GetLastModifyTime();
   const auto fsz = res.GetResult();
   if (fsz == 0) {
      RevPrint(rbuf, "|err=file is empty.");
      goto __ADD_DESC_HEAD_INFO_AND_RETURN;
   }
   if (this->LastPos_ >= fsz) {
      RevPrint(rbuf, "|err=file too small|fsz=", fsz, "|pos=", this->LastPos_);
      goto __ADD_DESC_HEAD_INFO_AND_RETURN;
   }
   if ((exitAutoRun.Loader_ = this->OnBeforeLoad(fsz, monFlag)).get() == nullptr) {
      RevPrint(rbuf, "|err=Not support|mon=", monFlag);
      goto __ADD_DESC_HEAD_INFO_AND_RETURN;
   }
   const size_t         kRemainSize = this->LastRemain_.size();
   const File::PosType  kRdSize = (fsz - this->LastPos_) + kRemainSize;
   const size_t   kMaxBufSize = 1024 * 1024 + kRemainSize;
   ByteVector     fbuf;
   char* const    pbuf = reinterpret_cast<char*>(fbuf.alloc(kRdSize > kMaxBufSize ? kMaxBufSize : kRdSize));
   size_t         bufofs = 0;
   if (kRemainSize > 0) {
      memcpy(pbuf, this->LastRemain_.c_str(), kRemainSize);
      this->LastRemain_.clear();
      bufofs = kRemainSize;
   }
   for (;;) {
      auto rdsz = fsz - this->LastPos_;
      if (rdsz > kMaxBufSize - bufofs)
         rdsz = kMaxBufSize - bufofs;
      res = fd.Read(this->LastPos_, pbuf + bufofs, rdsz);
      if (res.IsError()) {
         errfnc = "Read";
         goto __ERROR_FNC;
      }
      this->LastPos_ += res.GetResult();
      rdsz = bufofs + res.GetResult();
      const bool isEOF = (this->LastPos_ >= fsz);
      bufofs = exitAutoRun.Loader_->OnLoadBlock(pbuf, rdsz, isEOF);
      if (bufofs >= rdsz || (isEOF && bufofs > 0)) {
         if (isEOF && monFlag == FileImpMonitorFlag::AddTail && bufofs < kMaxBufSize) {
            this->LastRemain_.assign(pbuf, rdsz);
            break;
         }
         // 讀了一大塊 buffer, 完全無法處理? (沒有換行?)
         RevPrint(rbuf, "|err=Unknown data|pos=", this->LastPos_ - bufofs,
                  "|LnCount=", exitAutoRun.Loader_->LnCount_);
         goto __ADD_DESC_HEAD_INFO_AND_RETURN;
      }
      if (isEOF)
         break;
      if (bufofs)
         memmove(pbuf, pbuf + rdsz - bufofs, bufofs);
   }
   this->OnAfterLoad(rbuf, exitAutoRun.Loader_);
   if (monFlag == FileImpMonitorFlag::AddTail)
      RevPrint(rbuf, "(add)");
   RevPrint(rbuf,
            "|ftime=", this->LastFileTime_, FmtTS{"f-T.+'L'"},
            "|fsize=", fsz,
            "|LnCount=", exitAutoRun.Loader_->LnCount_);
   goto __ADD_DESC_HEAD_INFO_AND_RETURN;
}
void FileImpSeed::ClearReload(ConfigLocker&& lk) {
   (void)lk;
   this->ClearAddTailRemain();
   this->LastFileTime_.Assign0();
}
void FileImpSeed::SetForceLoadOnce(ConfigLocker&& lk, bool v) {
   if ((this->IsForceLoadOnce_ = v) == true)
      static_cast<FileImpMgr*>(&this->OwnerTree_.ConfigMgr_)->CheckStartMonitor(std::move(lk));
}
//--------------------------------------------------------------------------//
FileImpLoader::~FileImpLoader() {
   fon9_LOG_INFO("FileImpSeed.~Loader|loader=", ToPtr(this));
}
size_t FileImpLoader::OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) {
   return this->ParseLine(pbuf, bufsz, isEOF);
}
size_t FileImpLoader::ParseLine(char* pbuf, size_t bufsz, bool isEOF) {
   while (bufsz > 0) {
      switch (*pbuf) {
      case '\n': case '\r': // 移除無用的 '\n' '\r'
         ++pbuf;
         --bufsz;
         continue;
      }
      char* pend = static_cast<char*>(memchr(pbuf, '\n', bufsz));
      if (pend) {
         ++pend;
         size_t lnsz = static_cast<size_t>(pend - pbuf);
         this->OnLoadLine(pbuf, lnsz, false);
         bufsz -= lnsz;
         pbuf = pend;
         ++this->LnCount_;
      }
      else {
         if (!isEOF)
            return bufsz;
         this->OnLoadLine(pbuf, bufsz, true);
         ++this->LnCount_;
         return 0;
      }
   }
   return 0;
}
size_t FileImpLoader::ParseBlock(char* pbuf, size_t bufsz, bool isEOF, size_t lnsz) {
   while (bufsz >= lnsz) {
      this->OnLoadLine(pbuf, lnsz, isEOF && bufsz == lnsz);
      ++this->LnCount_;
      bufsz -= lnsz;
      pbuf += lnsz;
   }
   return bufsz;
}
void FileImpLoader::OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) {
   (void)pbuf; (void)bufsz; (void)isEOF;
}

} } // namespaces
