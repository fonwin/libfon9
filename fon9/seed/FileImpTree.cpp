// \file fon9/seed/FileImpTree.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/FileImpTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include <sys/stat.h>

namespace fon9 { namespace seed {

static inline void LastFileTime_SetAsOpenErr(TimeStamp& lastFileTime) {
   lastFileTime.Assign0();
}
static inline bool LastFileTime_IsOpenErr(TimeStamp lastFileTime) {
   return lastFileTime.IsZero();
}
static inline void LastFileTime_SetAsWaitReload(TimeStamp& lastFileTime) {
   lastFileTime.AssignNull();
}
static inline bool LastFileTime_IsWaitReload(TimeStamp lastFileTime) {
   return lastFileTime.IsNullOrZero();
}
//--------------------------------------------------------------------------//
FileImpMgr::~FileImpMgr() {
   this->MonitorTimer_.DisposeAndWait();
   this->DisposeAndWait();
}
TreeSP FileImpMgr::GetSapling() {
   return &this->GetConfigSapling();
}
StrView FileImpMgr::Name() const {
   return &this->Name_;
}
void FileImpMgr::MaConfigMgr_AddRef() {
   intrusive_ptr_add_ref(this);
}
void FileImpMgr::MaConfigMgr_Release() {
   intrusive_ptr_release(this);
}
void FileImpMgr::StopAndWait_SchTask() {
   base::StopAndWait_SchTask();
   this->MonitorTimer_.StopAndWait();
}

void FileImpMgr::OnSeedCommand(SeedOpResult&          res,
                               StrView                cmdln,
                               FnCommandResultHandler resHandler,
                               ConfigLocker&&         ulk,
                               SeedVisitor*           visitor) {
   (void)visitor; (void)ulk; assert(!ulk.owns_lock());
   res.OpResult_ = OpResult::no_error;
   fon9::StrTrim(&cmdln);
   if (cmdln == "reload") {
      this->LoadAll("cmd", UtcNow());
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
void FileImpMgr::LoadAll(StrView cause, TimeStamp forChkSch) {
   this->MonitorTimer_.StopAndWait();
   StrView itxt = (forChkSch.IsNullOrZero() ? StrView{"LoadAll"} : StrView{"LoadChkSch"});
   fon9_LOG_INFO("FileImpMgr|Info=", itxt, ".Start"
                 "|Cause=", cause,
                 "|Name=", this->OwnerTree_.ConfigMgr_.Name(), '/', this->Name_);
   this->GetFileImpSapling().LoadAll(forChkSch);
   this->SetNextTimeInfo(this->GetNextSchInTime(), ToStrView(RevPrintTo<std::string>(cause, ':', itxt, ".Done")));
   this->MonitorTimer_.RunAfter(TimeInterval_Second(1));
}
void FileImpMgr::OnSchTask_StateChanged(bool isInSch) {
   if (isInSch)
      this->MonitorTimer_.RunAfter(TimeInterval{});
}
void FileImpMgr::OnSchCfgChanged(StrView cfgstr) {
   // 初次啟動, 且沒有 SchIn;
   // if (this->Start(cfgstr) == SchSt::Unknown)
   //    this->LoadAll("Startup", UtcNow());
   // 20210224: 即使是初次啟動, 仍應要在 [排程時間內] 才需要載入.
   // 所以只要啟動 [排程檢查] 即可.
   this->Start(cfgstr);
}
void FileImpMgr::CheckStartMonitor(FileImpTree::Locker&& lk) {
   lk.unlock();
   this->MonitorTimer_.RunAfter(TimeInterval_Second(1));
}
void FileImpMgr::OnMonitorTimer(TimerEntry* timer, TimeStamp now) {
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
   this->MonitorTimer_.RunAfter(TimeInterval_Second(1));
}
//--------------------------------------------------------------------------//
FileImpTree::FileImpTree(FileImpMgr& mgr, LayoutSP layout)
   : base{mgr, std::move(layout)} {
}
void FileImpTree::LoadAll(TimeStamp forChkSch) {
   Locker imps{this->Container_};
   for (size_t idx = 0; idx < imps->size(); ++idx) {
      NamedSeedSP seed = imps->sindex(idx);
      if (FileImpSeed* imp = dynamic_cast<FileImpSeed*>(seed.get())) {
         imps.unlock();
         imp->LoadFrom(std::move(imps), nullptr, forChkSch);
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
                                ConfigLocker&&         ulk,
                                SeedVisitor*           visitor) {
   (void)visitor;
   StrView cmd = StrFetchTrim(cmdln, &isspace);
   StrTrim(&cmdln);
   res.OpResult_ = OpResult::no_error;
   if (cmd == "reload") {
      // reload [fileName]
      this->LoadFrom(std::move(ulk), cmdln, TimeStamp{});
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
   char chMonFlag = static_cast<char>(fon9::toupper(fon9::StrTrimHead(&val).Get1st()));
   switch (static_cast<FileImpMonitorFlag>(chMonFlag)) {
   case FileImpMonitorFlag::AddTail:
   case FileImpMonitorFlag::Reload:
   case FileImpMonitorFlag::Exclude:
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
bool FileImpSeed::IsInSch(TimeStamp tm) {
   SchConfig schcfg{ToStrView(this->SchCfgStr_)};
   auto      schres = schcfg.Check(tm, SchSt::Unknown);
   return(schres.SchSt_ == SchSt::InSch);
}
TimeInterval FileImpSeed::MonitorCheck(ConfigLocker&& lk, TimeStamp now) {
   assert(lk.owns_lock());
   if (this->IsReloading_)
      return TimeInterval_Second(1);
   if (this->IsForceLoadOnce_) {
      this->Reload(std::move(lk), this->GetTitle(), true);
      return TimeInterval_Millisecond(1);
   }
   FileImpMonitorFlag monFlag = this->GetMonitorFlag(lk);
   bool               isNoMon = false;
   switch (monFlag) {
   case FileImpMonitorFlag::Exclude:
      return TimeInterval{};

   default:
   case FileImpMonitorFlag::None:
      isNoMon = true;
      /* fall through */
   case FileImpMonitorFlag::AddTail:
   case FileImpMonitorFlag::Reload:
      if (!this->IsInSch(now)) {
         // 現在是排程時間外, 在下次進入排程時間內時, 至少要載入一次.
         // 所以清除 this->LastFileTime_; 讓下次進入排程時間內時, 至少載入一次.
         LastFileTime_SetAsWaitReload(this->LastFileTime_);
         return TimeInterval_Second(1);
      }
      if (isNoMon && !LastFileTime_IsWaitReload(this->LastFileTime_)) {
         // 不用監控, 且已經有載入過, 則不用再檢查排程時間.
         return TimeInterval{};
      }

      TimeStamp      ftm;
      File::PosType  fsz;
   #ifdef fon9_WINDOWS
      WIN32_FILE_ATTRIBUTE_DATA fad;
      if (!::GetFileAttributesEx(this->GetTitle().c_str(), GetFileExInfoStandard, &fad))
         fsz = 0;
      else {
         ULARGE_INTEGER fsize;
         fsize.HighPart = fad.nFileSizeHigh;
         fsize.LowPart = fad.nFileSizeLow;
         fsz = fsize.QuadPart;
         ftm = ToTimeStamp(fad.ftLastWriteTime);
      }
   #else
      struct stat fst;
      if (stat(this->GetTitle().c_str(), &fst) != 0)
         fsz = 0;
      else {
         fsz = unsigned_cast(fst.st_size);
         ftm = ToTimeStamp(fst.st_mtim);
      }
   #endif
   #define kTimeInterval_FileNotChanged   TimeInterval_Millisecond(200)
      if (ftm.IsNullOrZero()) {                           // 現在檔案不存在?
         if (LastFileTime_IsOpenErr(this->LastFileTime_)) // 上次偵測時, 開檔失敗(檔案也不存在?).
            break;                                        // => 此時不用更新 this->Description;
         // 上次偵測時, 檔案存在(開檔成功), 但現在檔案不存在,
         // 繼續處理: 呼叫 this->Reload(); 將此狀況顯示在 this->Description;
      }
      else { // 現在檔案存在.
         if (LastFileTime_IsWaitReload(this->LastFileTime_)) {
            // 上次偵測時, 檔案不存在, 現在檔案出現了!
            // => 不用其餘判斷, 繼續處理: 呼叫 this->Reload(); 
         }
         else { // 現在檔案存在, 上次偵測時檔案也存在: 檢查檔案是否有異動.
            if (monFlag == FileImpMonitorFlag::AddTail) {
               if (fsz <= this->LastFileSize_)
                  return kTimeInterval_FileNotChanged;
            }
            else {
               assert(monFlag == FileImpMonitorFlag::Reload);
               if (this->LastFileTime_ == ftm && this->LastFileSize_ == fsz)
                  return kTimeInterval_FileNotChanged;
            }
         }
      }

      return this->Reload(std::move(lk), this->GetTitle(), false);
   }
   // Check file st error.
   return TimeInterval_Second(1);
}
void FileImpSeed::LoadFrom(ConfigLocker&& ulk, StrView fname, TimeStamp forChkSch) {
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
   this->ClearAddTailRemain();
   StrView desc;
   if (!forChkSch.IsNullOrZero() && this->GetMonitorFlag(ulk) == FileImpMonitorFlag::Exclude) {
      // !forChkSch.IsNullOrZero():表示需要檢查排程, 但 FileImpMonitorFlag::Exclude 表示不檢查排程;
      // => 此時不載入.
      desc = "|Exclude";
   }
   else if (forChkSch.IsNullOrZero() || this->IsInSch(forChkSch)) {
      this->Reload(std::move(ulk), std::move(strfn), true);
      assert(ulk.owns_lock());
   }
   else {
      desc = "|OutSch";
   }
   if (!desc.IsNullOrEmpty())
      this->SetDescription(RevPrintTo<std::string>(forChkSch + GetLocalTimeZoneOffset(), kFmtYMD_HH_MM_SS_us6, desc));
   if (isNeedsWriteConfig)
      this->OwnerTree_.WriteConfig(ulk, this);
}
TimeInterval FileImpSeed::Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) {
   this->IsForceLoadOnce_ = false;
   assert(lk.owns_lock());
   if (this->IsReloading_) {
      this->SetDescription("Loading");
      return TimeInterval_Second(1);
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
   const auto monFlag = this->GetMonitorFlag(lk);
   lk.unlock();
   if (isClearAddTailRemain || monFlag != FileImpMonitorFlag::AddTail) {
      this->ClearAddTailRemain();
   }
   fon9_LOG_INFO("FileImpSeed.Start|Name=", this->Name_, "|fname=", fname, "|pos=", this->LastPos_);

   File    fd;
   StrView errfnc;
   auto    res = fd.Open(fname, FileMode::Read);
   RevBufferList rbuf{256};
   TimeInterval  retval = TimeInterval_Second(1);
   if (res.IsError()) {
      errfnc = "Open";
      // TODO: 開檔失敗是否需要 this->ClearAddTailRemain()? 是否需要增加另一設定(FileImpMonitorFlag::AddTail_ErrClear)?
      LastFileTime_SetAsOpenErr(this->LastFileTime_);
   __ERROR_FNC:;
      RevPrint(rbuf, '|', errfnc, ' ', res);
   __ADD_DESC_HEAD_INFO_AND_RETURN:;
      RevPrint(rbuf, LocalNow(), kFmtYMD_HH_MM_SS_us6, "|fname=", fname);
      lk.lock();
      this->SetDescription(BufferTo<std::string>(rbuf.MoveOut()));
      this->OwnerTree_.UpdateConfigSeed(lk, *this, nullptr, nullptr);
      return retval;
   }
   res = fd.GetFileSize();
   if (res.IsError()) {
      errfnc = "GetFileSize";
      goto __ERROR_FNC;
   }
   const auto fsz = this->LastFileSize_ = res.GetResult();
   this->LastFileTime_ = fd.GetLastModifyTime();
   if (fsz == 0) {
      this->OnLoadEmptyFile();
      RevPrint(rbuf, "|err=file is empty.");
      goto __ADD_DESC_HEAD_INFO_AND_RETURN;
   }
   if (this->LastPos_ >= fsz) {
      RevPrint(rbuf, "|err=file too small|fsz=", fsz, "|pos=", this->LastPos_);
      goto __ADD_DESC_HEAD_INFO_AND_RETURN;
   }

   const size_t   kRemainSize = this->LastRemain_.size();
   File::PosType  kRdSize = (fsz - this->LastPos_) + kRemainSize;
   if ((exitAutoRun.Loader_ = this->OnBeforeLoad(rbuf, kRdSize, monFlag)).get() == nullptr) {
      RevPrint(rbuf,
               "|ftime=", this->LastFileTime_, kFmtYMD_HH_MM_SS_us_L,
               "|fsize=", fsz,
               "|mon=", monFlag);
      goto __ADD_DESC_HEAD_INFO_AND_RETURN;
   }
   if (LastFileTime_IsWaitReload(this->LastFileTime_)) { // 在 this->OnBeforeLoad() 呼叫了 this->ClearReload()?
      RevPrint(rbuf, "|ClearReload.");
      goto __ADD_DESC_HEAD_INFO_AND_RETURN;  // 等候下次載入時機, 重新載入.
   }

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
            this->LastRemain_.assign(pbuf + rdsz - bufofs, bufofs);
            break;
         }
         // 讀了一大塊(kMaxBufSize? 或 isEOF) buffer, 完全無法處理? (沒有換行?)
         RevPrint(rbuf, "|err=Unknown data|pos=", this->LastPos_ - bufofs,
                  "|LnCount=", exitAutoRun.Loader_->LnCount_);
         goto __ADD_DESC_HEAD_INFO_AND_RETURN;
      }
      if (isEOF)
         break;
      if (bufofs)
         memmove(pbuf, pbuf + rdsz - bufofs, bufofs);
   }
   this->OnAfterLoad(rbuf, exitAutoRun.Loader_, monFlag);
   if (monFlag == FileImpMonitorFlag::AddTail)
      RevPrint(rbuf, "(add)");
   RevPrint(rbuf,
            "|ftime=", this->LastFileTime_, kFmtYMD_HH_MM_SS_us_L,
            "|fsize=", fsz,
            "|LnCount=", exitAutoRun.Loader_->LnCount_);
   // 讀取成功, 接下來延遲 1ms, 再次嘗試, 如果再次嘗試時檔案內容沒變, 才會延遲 kTimeInterval_FileNotChanged
   retval = TimeInterval_Millisecond(1);
   goto __ADD_DESC_HEAD_INFO_AND_RETURN;
}
void FileImpSeed::OnLoadEmptyFile() {
}
void FileImpSeed::ClearReload(ConfigLocker&& lk) {
   (void)lk;
   this->ClearAddTailRemain();
   LastFileTime_SetAsWaitReload(this->LastFileTime_);
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
      if (char* pend = static_cast<char*>(memchr(pbuf, '\n', bufsz))) {
         ++pend;
         size_t lnsz = static_cast<size_t>(pend - pbuf);
         bufsz -= lnsz;
         this->OnLoadLine(pbuf, lnsz, (isEOF && bufsz == 0));
         pbuf = pend;
         ++this->LnCount_;
      }
      else { // 沒找到換行字元?
         // 不論是否為 isEOF, 都必須要有換行字元.
         // - 否則格式不合.
         // - 也有可能為 AddTail, 尚未收到完整的一行.
         return bufsz;
         // if (!isEOF)
         //    return bufsz;
         // this->OnLoadLine(pbuf, bufsz, true);
         // ++this->LnCount_;
         // return 0;
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
