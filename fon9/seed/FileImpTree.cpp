// \file fon9/seed/FileImpTree.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/FileImpTree.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace fon9 { namespace seed {

FileImpMgr::~FileImpMgr() {
   this->DisposeAndWait();
}
TreeSP FileImpMgr::GetSapling() {
   return this->Sapling_;
}
void FileImpMgr::OnParentTreeClear(Tree& tree) {
   this->DisposeAndWait();
   base::OnParentTreeClear(tree);
}
void FileImpMgr::OnSeedCommand(SeedOpResult& res,
                               StrView cmdln,
                               FnCommandResultHandler resHandler,
                               MaTreeBase::Locker&& ulk) {
   (void)ulk; assert(!ulk.owns_lock());
   res.OpResult_ = OpResult::no_error;
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
TimeStamp FileImpMgr::GetNextSchInTime() {
   SchImpl::Locker schImpl{this->SchImpl_};
   TimeStamp       tmNext = schImpl->Config_.GetNextSchIn(UtcNow());
   return(tmNext.GetOrigValue() == 0 ? TimeStamp::Null() : tmNext);
}
void FileImpMgr::SetNextTimeInfo(TimeStamp tmNext, StrView exInfo) {
   const FmtTS tmNextFmt{"f-T+'L'"};
   std::string desc = RevPrintTo<std::string>(
      LocalNow(), FmtTS{"f-T.6"}, '|', exInfo, "|Next=", tmNext, tmNextFmt);
   auto locker{this->ConfigTree_.Lock()};
   fon9_LOG_INFO("FileImpMgr|Info=", exInfo,
                 "|Name=", this->ConfigTree_.ConfigMgr_.Name_, '/', this->Name_,
                 "|Next=", tmNext, tmNextFmt,
                 "|SchCfg={", this->GetConfigValue(locker), '}');
   this->ConfigTree_.UpdateConfigSeed(locker, *this, nullptr, &desc);
}
void FileImpMgr::LoadAll() {
   fon9_LOG_INFO("FileImpMgr|Info=LoadAll.Start"
                 "|Name=", this->ConfigTree_.ConfigMgr_.Name_, '/', this->Name_);
   static_cast<FileImpTree*>(this->Sapling_.get())->LoadAll();
   this->SetNextTimeInfo(this->GetNextSchInTime(), "LoadAll.Done");
}
void FileImpMgr::OnSchTask_StateChanged(bool isInSch) {
   if (isInSch) {
      intrusive_ptr<FileImpMgr> pthis{this};
      GetDefaultThreadPool().EmplaceMessage(std::bind(&FileImpMgr::LoadAll, pthis));
   }
}
void FileImpMgr::OnSchTask_NextCheckTime(const SchConfig::CheckResult&) {
   this->SetNextTimeInfo(this->GetNextSchInTime(), "NextCheckTime");
}
void FileImpMgr::OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val) {
   CharVector cfgstr{val};
   lk.unlock();
   if (this->Start(ToStrView(cfgstr)) == SchSt::Unknown) // 初次啟動時載入.
      this->LoadAll();
   // ----- 取得 Sch設定 的正規化字串.
   {
      SchImpl::Locker schImpl{this->SchImpl_};
      cfgstr = RevPrintTo<CharVector>(schImpl->Config_);
   }
   // -----
   base::OnConfigValueChanged(std::move(lk), ToStrView(cfgstr));
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
void FileImpSeed::OnSeedCommand(SeedOpResult& res,
                                StrView cmdln,
                                FnCommandResultHandler resHandler,
                                MaTreeBase::Locker&& ulk) {
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
void FileImpSeed::LoadFrom(MaTreeBase::Locker&& ulk, const StrView fname) {
   ulk.lock();
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
   std::string strfn = fname.empty() ? this->GetTitle() : fname.ToString();
   if (this->GetTitle().empty())
      this->SetTitle(strfn);
   ulk.unlock();
   fon9_LOG_INFO("FileImpSeed.Start|Name=", this->Name_, "|fname=", strfn);

   File    fd;
   StrView errfnc;
   auto    res = fd.Open(strfn, FileMode::Read);
   RevBufferList rbuf{256};
   if (res.IsError()) {
      errfnc = "Open";
   __ERROR_FNC:;
      RevPrint(rbuf, '|', errfnc, ' ', res);
   __ERROR_ADD_HEAD:;
      RevPrint(rbuf, LocalNow(), FmtTS{"f-T."}, "|fname=", strfn);
      ulk.lock();
      this->SetDescription(BufferTo<std::string>(rbuf.MoveOut()));
      return;
   }
   res = fd.GetFileSize();
   if (res.IsError()) {
      errfnc = "GetFileSize";
      goto __ERROR_FNC;
   }
   const auto fsz = res.GetResult();
   if (fsz == 0) {
      RevPrint(rbuf, "|err=file is empty.");
      goto __ERROR_ADD_HEAD;
   }
   const size_t    kMaxBufSize = 1024 * 1024;
   File::PosType   fpos = 0;
   ByteVector      fbuf;
   char*           pbuf = reinterpret_cast<char*>(fbuf.alloc(fsz > kMaxBufSize ? kMaxBufSize : fsz));
   size_t          bufofs = 0;
   exitAutoRun.Loader_ = this->OnBeforeLoad(fsz);
   for (;;) {
      auto rdsz = fsz - fpos;
      if (rdsz > kMaxBufSize - bufofs)
         rdsz = kMaxBufSize - bufofs;
      res = fd.Read(fpos, pbuf + bufofs, rdsz);
      if (res.IsError()) {
         errfnc = "Read";
         goto __ERROR_FNC;
      }
      fpos += res.GetResult();
      rdsz = bufofs + res.GetResult();
      const bool isEOF = (fpos >= fsz);
      bufofs = exitAutoRun.Loader_->OnLoadBlock(pbuf, rdsz, isEOF);
      if (bufofs >= rdsz || (isEOF && bufofs > 0)) {
         // 讀了一大塊 buffer, 完全無法處理? (沒有換行?)
         RevPrint(rbuf, "|err=Unknown data|pos=", fpos - bufofs,
                  "|LnCount=", exitAutoRun.Loader_->LnCount_);
         goto __ERROR_ADD_HEAD;
      }
      if (isEOF)
         break;
      if (bufofs)
         memmove(pbuf, pbuf + rdsz - bufofs, bufofs);
   }
   this->OnAfterLoad(rbuf, exitAutoRun.Loader_);
   RevPrint(rbuf, LocalNow(), FmtTS{"f-T.6"},
            "|ftime=", fd.GetLastModifyTime(), FmtTS{"f-T.+'L'"},
            "|fsize=", fsz,
            "|LnCount=", exitAutoRun.Loader_->LnCount_);
   ulk.lock();
   if (!fname.empty())
      this->SetTitle(std::move(strfn));
   this->SetDescription(BufferTo<std::string>(rbuf.MoveOut()));
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
