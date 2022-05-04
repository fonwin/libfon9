// \file f9twf/ExgMapMgr.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMapMgr.hpp"

namespace f9twf {
using namespace fon9::seed;

f9twf_API void TwfSysImp_UnsafeSetDefaultSchCfg(ExgSystemType sysType, fon9::seed::FileImpSeed& imp) {
   if (ExgSystemTypeIsAfterHour(sysType))
      // 夜盤P08: 13:00-05:00(夜盤開始前..OMS換日前), 若有異動主動匯入.
      imp.UnsafeSetSchCfgStr("Start=130000|End=050000");
   else
      // 日盤P08: 07:00-12:30(OMS換日後..夜盤開始前), 若有異動主動匯入.
      imp.UnsafeSetSchCfgStr("Start=070000|End=123000");
}
//--------------------------------------------------------------------------//
void ExgMapMgr::SetWaitingDescription(fon9::Named& impSeed, fon9::StrView cstrInfo, ExgSystemType sysType) {
   const auto  cursz = impSeed.GetDescription().size();
   const char* curmsg = impSeed.GetDescription().c_str();
   if (cursz > cstrInfo.size() && memcmp(curmsg, cstrInfo.begin(), cstrInfo.size()) == 0) {
      if (sysType == ExgSystemType{})
         return;
      if( curmsg[cursz - 2] == static_cast<char>('0' + fon9::cast_to_underlying(sysType) / 10)
       && curmsg[cursz - 1] == static_cast<char>('0' + fon9::cast_to_underlying(sysType) % 10) )
         return;
   }
   std::string info = cstrInfo.ToString();
   if (sysType != ExgSystemType{}) {
      info.push_back(static_cast<char>('0' + fon9::cast_to_underlying(sysType) / 10));
      info.push_back(static_cast<char>('0' + fon9::cast_to_underlying(sysType) % 10));
   }
   impSeed.SetDescription(std::move(info));
}
//--------------------------------------------------------------------------//
void ExgMapMgr::ImpSeedBase::OnAfterLoad(fon9::RevBuffer& rbufDesp, FileImpLoaderSP loader, FileImpMonitorFlag monFlag) {
   (void)rbufDesp; (void)loader; (void)monFlag;
}
void ExgMapMgr::ImpSeedForceLoadSesNormal::ClearReload(ConfigLocker&& lk) {
   this->SetDescription(std::string{});
   base::ClearReload(std::move(lk));
   if (this->GetMonitorFlag(lk) != FileImpMonitorFlag::Exclude // 有可能不載入 PA8;
       && !ExgSystemTypeIsAfterHour(this->ExgSystemType_)) {
      // 有些檔案是 [必要檔], 例如: 日盤P08, 即使是 OutSch, 在重啟時也要載入一次.
      // 所以這裡設定強制載入.
      this->SetForceLoadOnce(std::move(lk), true);
   }
}
//--------------------------------------------------------------------------//
using ExgMapMgr_ImpSeed = ExgMapMgr::ImpSeedBase;

template <class Loader>
struct ExgMapMgr_ImpSeedT : public ExgMapMgr_ImpSeed {
   fon9_NON_COPY_NON_MOVE(ExgMapMgr_ImpSeedT);
   using base = ExgMapMgr_ImpSeed;
   using base::base;
   FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) override {
      (void)rbufDesp;
      return new Loader(*this, addSize, monFlag);
   }
};
struct ExgMapMgr::P06Loader : public FileImpLoader {
   fon9_NON_COPY_NON_MOVE(P06Loader);
   ExgMapMgr_ImpSeed&   Owner_;
   ExgMapBrkFcmId       MyMap_;
   ExgMapBrkFcmId*      PMapBrkFcmId_;
   P06Loader(ExgMapMgr_ImpSeed& owner, size_t addSize, FileImpMonitorFlag monFlag)
      : Owner_{owner} {
      if (monFlag == FileImpMonitorFlag::AddTail) {
         Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
         this->PMapBrkFcmId_ = &maps->MapBrkFcmId_;
      }
      else {
         this->PMapBrkFcmId_ = &this->MyMap_;
         this->MyMap_.reserve(addSize / sizeof(TmpP06) + 1);
      }
   }
   ~P06Loader() {
      ExgMapMgr&   exgMapMgr = this->Owner_.GetExgMapMgr();
      Maps::Locker maps{exgMapMgr.Maps_};
      if (this->PMapBrkFcmId_ == &this->MyMap_)
         maps->MapBrkFcmId_.swap(this->MyMap_);
      exgMapMgr.OnP06Updated(maps->MapBrkFcmId_, std::move(maps));
   }
   size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
      if (this->PMapBrkFcmId_ == &this->MyMap_)
         return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(TmpP06));
      Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
      return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(TmpP06));
   }
   void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
      (void)bufsz; (void)isEOF;
      assert(bufsz == sizeof(TmpP06));
      this->PMapBrkFcmId_->AddItem(*reinterpret_cast<TmpP06*>(pbuf));
   }
};
struct ExgMapMgr::P07Loader : public FileImpLoader {
   fon9_NON_COPY_NON_MOVE(P07Loader);
   ExgMapMgr_ImpSeed&      Owner_;
   ExgMapSessionDn         MyMap_;
   ExgMapSessionDn*        PMapSessionDn_;
   const ExgMapBrkFcmId*   PMapBrkFcmId_;
   P07Loader(ExgMapMgr_ImpSeed& owner, size_t addSize, FileImpMonitorFlag monFlag)
      : Owner_{owner} {
      if (monFlag == FileImpMonitorFlag::AddTail) {
         Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
         this->PMapSessionDn_ = &maps->MapSessionDn_[ExgSystemTypeToIndex(this->Owner_.ExgSystemType_)];
      }
      else {
         this->PMapSessionDn_ = &this->MyMap_;
         this->MyMap_.reserve(addSize / sizeof("f906000.session1.tmp.fut.taifex,30002"));
      }
   }
   ~P07Loader() {
      if (this->PMapSessionDn_ == &this->MyMap_) {
         this->MyMap_.shrink_to_fit();
         Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
         maps->MapSessionDn_[ExgSystemTypeToIndex(this->Owner_.ExgSystemType_)].swap(this->MyMap_);
      }
   }
   size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
      Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
      this->PMapBrkFcmId_ = &maps->MapBrkFcmId_;
      return this->ParseLine(pbuf, bufsz, isEOF && (this->PMapSessionDn_ == &this->MyMap_));
   }
   void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
      (void)isEOF;
      this->PMapSessionDn_->FeedLine(*this->PMapBrkFcmId_, fon9::StrView{pbuf, bufsz});
   }
};
//--------------------------------------------------------------------------//
struct ExgMapMgr::ImpSeedP08 : public ExgMapMgr::ImpSeedForceLoadSesNormal {
   fon9_NON_COPY_NON_MOVE(ImpSeedP08);
   using base = ExgMapMgr::ImpSeedForceLoadSesNormal;

   template <class... ArgsT>
   ImpSeedP08(ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...) {
      this->Ctor();
   }
   void Ctor() {
      this->SetDescription("Waiting TDay");
      this->SetDefaultSchCfg();
   }

   struct Loader : public FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedP08&       Owner_;
      const size_t      IdSize_;
      P08Recs*          P08Recs_;
      fon9::TimeStamp   UpdatedTime_;

      Loader(ImpSeedP08& owner, size_t addSize)
         : Owner_{owner}
         , IdSize_{owner.Name_[1] == 'A' ? sizeof(ExgProdIdL) : sizeof(ExgProdIdS)}
         , UpdatedTime_{owner.LastFileTime()} {
         size_t       rsz = addSize / (this->IdSize_ + sizeof(TmpP08Base)) + 1;
         Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
         this->P08Recs_ = &maps->MapP08Recs_[ExgSystemTypeToIndex(this->Owner_.ExgSystemType_)];
         this->P08Recs_->reserve(rsz);
         if (this->IdSize_ == sizeof(ExgProdIdL))
            this->P08Recs_->UpdatedTimeL_ = this->UpdatedTime_;
         else
            this->P08Recs_->UpdatedTimeS_ = this->UpdatedTime_;
      }
      ~Loader() {
         ExgMapMgr& exgMapMgr = this->Owner_.GetExgMapMgr();
         exgMapMgr.OnP08Updated(*this->P08Recs_,
                                this->Owner_.ExgSystemType_,
                                exgMapMgr.Maps_.ConstLock());
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
         return this->ParseBlock(pbuf, bufsz, isEOF, this->IdSize_ + sizeof(TmpP08Base));
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)bufsz; (void)isEOF;
         assert(bufsz == this->IdSize_ + sizeof(TmpP08Base));
         const TmpP08Fields*  p08flds = reinterpret_cast<const TmpP08Fields*>(pbuf + this->IdSize_);
         const unsigned       pseq = fon9::Pic9StrTo<unsigned>(p08flds->pseq_);
         const bool           isNew = (pseq >= this->P08Recs_->size());
         if (isNew) {
            if (pseq >= fon9::DecDivisor<unsigned, p08flds->pseq_.max_size()>::Divisor)
               return;
            this->P08Recs_->resize(pseq + 1);
         }
         P08Rec*  prec = &(*this->P08Recs_)[pseq];
         size_t   idLenNew = static_cast<size_t>(fon9::StrFindTrimTail(pbuf, pbuf + this->IdSize_) - pbuf);
         if (!isNew) {
            const auto* pOldId = (this->IdSize_ == sizeof(ExgProdIdL)
                                  ? prec->LongId_.LenChars()
                                  : prec->ShortId_.LenChars());
            if (fon9_LIKELY(idLenNew == *pOldId && memcmp(pbuf, pOldId + 1, idLenNew) == 0)) {
               if (fon9_LIKELY(memcmp(&prec->Fields_, p08flds, sizeof(*p08flds)) == 0))
                  return;
            }
            else if (*pOldId != 0) { // 某個序號(pseq)的商品Id改變了, 應清除「長短商品Id」, 重新設定.
               prec->LongId_.clear();
               prec->ShortId_.clear();
            }
         }
         if (this->IdSize_ == sizeof(ExgProdIdL))
            prec->LongId_.CopyFrom(pbuf, idLenNew);
         else
            prec->ShortId_.CopyFrom(pbuf, idLenNew);
         prec->Fields_ = *p08flds;
         prec->UpdatedTime_ = this->UpdatedTime_;
      }
   };
   FileImpLoaderSP OnBeforeLoad(fon9::RevBuffer& rbufDesp, uint64_t addSize, FileImpMonitorFlag monFlag) override {
      if (monFlag == FileImpMonitorFlag::AddTail) {
         fon9::RevPrint(rbufDesp, "|err=", this->Name_, " not support AddTail.");
         return nullptr;
      }
      return this->MakeLoader(addSize);
   }
   virtual FileImpLoaderSP MakeLoader(uint64_t addSize) {
      return new Loader(*this, addSize);
   }
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (!this->GetExgMapMgr().TDay_.IsNullOrZero())
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
};
//--------------------------------------------------------------------------//
struct ExgMapMgr::ImpSeedP09 : public ImpSeedP08 {
   fon9_NON_COPY_NON_MOVE(ImpSeedP09);
   using base = ImpSeedP08;
   using base::base;

   struct Loader : public FileImpLoader {
      fon9_NON_COPY_NON_MOVE(Loader);
      ImpSeedP09&       Owner_;
      const size_t      IdSize_;
      P09Recs*          P09Recs_;
      fon9::TimeStamp   UpdatedTime_;
   
      Loader(ImpSeedP09& owner, size_t addSize)
         : Owner_{owner}
         , IdSize_{// owner.Name_[1] == 'A' ? sizeof(ExgContractIdL::Id) : 
                   sizeof(ExgContractIdS::Id)}
         , UpdatedTime_{owner.LastFileTime()} {
         assert(owner.Name_[1] == '0'); // 僅支援 P09, 不支援 PA9;
         size_t       rsz = addSize / (this->IdSize_ + sizeof(TmpP09Base)) + 1;
         Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
         this->P09Recs_ = &maps->MapP09Recs_[ExgSystemTypeToIndex(this->Owner_.ExgSystemType_)];
         this->P09Recs_->reserve(rsz);
      }
      ~Loader() {
         ExgMapMgr& exgMapMgr = this->Owner_.GetExgMapMgr();
         exgMapMgr.OnP09Updated(*this->P09Recs_,
                                this->Owner_.ExgSystemType_,
                                exgMapMgr.Maps_.ConstLock());
      }
      size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
         Maps::Locker maps{this->Owner_.GetExgMapMgr().Maps_};
         return this->ParseBlock(pbuf, bufsz, isEOF, this->IdSize_ + sizeof(TmpP09Base));
      }
      void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
         (void)bufsz; (void)isEOF;
         assert(bufsz == this->IdSize_ + sizeof(TmpP09Base) && bufsz == sizeof(TmpP09));
         this->P09Recs_->emplace_back(*reinterpret_cast<const TmpP09*>(pbuf));
      }
   };
   FileImpLoaderSP MakeLoader(uint64_t addSize) override {
      return new Loader(*this, addSize);
   }
   fon9::TimeInterval Reload(ConfigLocker&& lk, std::string fname, bool isClearAddTailRemain) override {
      if (this->GetExgMapMgr().IsCurrencyConfigNeedlessOrReady(lk, *this))
         return base::Reload(std::move(lk), std::move(fname), isClearAddTailRemain);
      return fon9::TimeInterval_Second(1);
   }
};
//--------------------------------------------------------------------------//
FileImpTreeSP ExgMapMgr::MakeSapling(ExgMapMgr& rthis) {
   using namespace fon9::seed;
   FileImpTreeSP retval{new FileImpTree{rthis}};

   retval->Add(new ExgMapMgr_ImpSeedT<P06Loader>(ExgSystemType{}, *retval, "P06", "./tmpsave/P06"));

   // P07/PA7 二選一載入, 因為 PA7 包含 P07; 如果有需要使用備援的 dn, 則使用 PA7;
   ExgMapMgr_AddImpSeed_S2FO(*retval, ExgMapMgr_ImpSeedT<P07Loader>, "P07");

   // -------------------------------------
   // 根據期交所文件(TCPIP_TMP_v2.11.0.pdf):
   // 因應「盤後交易」某些商品資訊，必須待「一般交易時段」結束方可產生，
   // P08 檔案為一盤產生多次，請期貨商資訊系統收到通知時重新更新資料。
   // - 「盤後第一次」產生時間約為 14:35。
   //    - 因 RHF、RTF 等商品資訊尚未確定，此次產生之 P08 不包含資料不確定之商品(如 RHF、RTF 等)。
   // - 「盤後第二次」產生時間約為 17:00。
   //    - 此次產生為完整之夜盤 P08 檔案。
   // -------------------------------------
   // ExgMapMgr 匯入 P08 的作法:
   // - 由 OMS 啟動, 當 TDay Changed 或系統初次啟動:
   //   - 清空已匯入的 P08 內容.
   //   - 重新載入全部 P08;
   // - ImpSeedP08「必須」設定 Sch:
   //   - 日盤P08: 07:00-12:30(OMS換日後..夜盤開始前), 若有異動主動匯入.
   //   - 夜盤P08: 13:00-05:00(夜盤開始前..OMS換日前), 若有異動主動匯入.
   //   - 把時間錯開, 避免日夜盤P08有相同商品時, 要以哪個為主的問題.
   //   - 以後期交所會不會「日盤P08」、「夜盤P08」同時異動?
   //     => 目前看起來不會, 現在的情況是:
   //        -「日盤P08」每天產生一次.
   //        -「夜盤P08」每天產生多次.
   //        - 所以只要在首次「夜盤P08」產生前, 停止「日盤P08」的匯入,
   //          就可以避免「相同商品Id」的資料被覆蓋的問題.
   //     => 如果以後真有這種情境, 相信 P08 的設計會有調整.
   // - 目前(20191015/TCPIP_TMP_v2.11.0)流程群組的開收盤時間如下:
   //       開盤  收盤
   //     1 08:45 13:45
   //     2 08:45 12:00
   //     3 08:45 13:30
   //     4 08:45 11:00
   //     5 08:45 16:15
   //     6 08:00 16:15
   //     7 08:45 18:15
   //     8 08:45 18:00
   //     9 08:45 14:00
   //    10 15:00 隔日 5:00
   //    11 17:25 隔日 5:00
   //    12 15:00 (夏令時段)隔日 02:30 / (冬令時段)隔日 03:30
   // -------------------------------------
   // - 是否需要等 P08(ShortId)+PA8(LongId) 都載入後才觸發通知?
   //   - P08/PA8 的 Sch 是否為 AlwaysOut? 若是, 則不等.
   // - 或是個別通知? 不考慮另一個是否已載入.
   //   - 因為還需要配合帳務系統的結算.
   //   - 所以由訂閱者自行決定, 以「長Id」或「短Id」為主.
   // - 期交所行情也有用不同的 Multicast 群組,
   //   來播送「長Id」或「短Id」的行情.
   // -------------------------------------
   ExgMapMgr_AddImpSeed_S2FO(*retval, ImpSeedP08, "P08");
   ExgMapMgr_AddImpSeed_S2FO(*retval, ImpSeedP08, "PA8");
   // -------------------------
   ExgMapMgr_AddImpSeed_S2FO(*retval, ImpSeedP09, "P09");
   return retval;
}
//--------------------------------------------------------------------------//
bool ExgMapMgr::IsP08Ready(ExgSystemType sys) const {
   auto  maps = this->Lock();
   auto& p08s = maps->MapP08Recs_[ExgSystemTypeToIndex(sys)];
   return(!p08s.empty() && !p08s.UpdatedTimeS_.IsNullOrZero());
}
bool ExgMapMgr::IsP08Ready(ExgSystemType sys, const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const {
   (void)lk; assert(lk.owns_lock());
   if (this->IsP08Ready(sys))
      return true;
   SetWaitingDescription(impSeed, "Waiting P08_", sys);
   return false;
}
bool ExgMapMgr::IsP09Ready(ExgSystemType sys) const {
   auto maps = this->Lock();
   if (sys != ExgSystemType{})
      return !maps->MapP09Recs_[ExgSystemTypeToIndex(sys)].empty();
   return !maps->MapP09Recs_[ExgSystemTypeToIndex(ExgSystemType::FutNormal)].empty()
       && !maps->MapP09Recs_[ExgSystemTypeToIndex(ExgSystemType::OptNormal)].empty();
}
bool ExgMapMgr::IsP09Ready(ExgSystemType sys, const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const {
   (void)lk; assert(lk.owns_lock());
   if (this->IsP09Ready(sys))
      return true;
   SetWaitingDescription(impSeed, "Waiting P09_", sys);
   return false;
}
bool ExgMapMgr::IsP13Ready(ExgSystemType sys, const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const {
   (void)lk; assert(lk.owns_lock());
   if (!(this->IsMainContractRefReadyBits_ & (1 << f9twf::ExgSystemTypeToIndex(sys)))) {
      SetWaitingDescription(impSeed, "Waiting P13_", sys);
      return false;
   }
   return true;
}
bool ExgMapMgr::IsMainContractRefReady(const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const {
   (void)lk; assert(lk.owns_lock());
   if (!(this->IsMainContractRefReadyBits_ & (1 << f9twf::ExgSystemTypeCount()))) {
      SetWaitingDescription(impSeed, "Waiting ContractRef", ExgSystemType{});
      return false;
   }
   return(this->IsP13Ready(f9twf::ExgSystemType::FutNormal, lk, impSeed)
       && this->IsP13Ready(f9twf::ExgSystemType::OptNormal, lk, impSeed));
}
bool ExgMapMgr::IsCurrencyConfigNeedlessOrReady(const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const {
   (void)lk; assert(lk.owns_lock());
   if (this->IsNeedsCurrencyConfig_) {
      if (!this->IsCurrencyConfigReady_) {
         SetWaitingDescription(impSeed, "Waiting CurrencyConfig", ExgSystemType{});
         return false;
      }
   }
   return true;
}
void ExgMapMgr::OnP08Updated(const P08Recs& p08recs, ExgSystemType sysType, Maps::ConstLocker&& lk) {
   (void)p08recs; (void)sysType; (void)lk;
}
void ExgMapMgr::OnP09Updated(const P09Recs& p09recs, ExgSystemType sysType, Maps::ConstLocker&& lk) {
   (void)p09recs; (void)sysType; (void)lk;
}
void ExgMapMgr::OnP06Updated(const ExgMapBrkFcmId& mapBrkFcmId, Maps::Locker&& lk) {
   (void)mapBrkFcmId; (void)lk;
}
bool ExgMapMgr::AppendDn(ExgSystemType sys, const ExgLineTmpArgs& lineArgs, std::string& devcfg) const {
   unsigned idx = ExgSystemTypeToIndex(sys);
   if (idx >= ExgSystemTypeCount())
      return false;
   Maps::ConstLocker maps{this->Maps_};
   return maps->MapSessionDn_[idx].AppendDn(lineArgs, devcfg);
}
void ExgMapMgr::SetTDay(fon9::TimeStamp tday) {
   {
      Maps::Locker maps{this->Maps_};
      for (auto& p08recs : maps->MapP08Recs_) {
         p08recs.clear();
         p08recs.UpdatedTimeL_.AssignNull();
         p08recs.UpdatedTimeS_.AssignNull();
      }
      for (auto& p09recs : maps->MapP09Recs_) {
         p09recs.clear();
      }
      this->TDay_ = tday;
      this->IsMainContractRefReadyBits_ = 0;
      this->IsCurrencyConfigReady_ = false;
   }
   this->ClearReloadAll();
   this->LoadAll("SetTDay", fon9::UtcNow());
}

} // namespaces
