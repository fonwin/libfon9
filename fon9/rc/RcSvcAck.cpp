// \file fon9/rc/RcSvcAck.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSvcAck.hpp"

namespace fon9 { namespace rc {

void RcSvcTreeRejector::Run(svc::TreeLocker& maplk, RcClientSession& ses, svc::TreeRec& tree) {
   rpt.TreePath_ = ToStrView(tree.TreePath_).ToCStrView();
   rpt.Layout_ = &tree.LayoutC_;
   rpt.Tab_ = &tab;
   for (auto& preqSP : tree.PendingReqs_) {
      RcSvcReq& preq = *preqSP;
      preq.RejectPending(maplk);
      if (preq.Handler_.FnOnReport_) {
         preq.AssignRptTabSeed(rpt);
         preq.Handler_.FnOnReport_(&ses, &rpt);
      }
   }
   for (auto& pod : tree.PodMap_) {
      for (unsigned L = rpt.Layout_->TabCount_; L > 0;) {
         svc::SeedRec* seed = pod.second.Seeds_[--L].get();
         rpt.SeedKey_ = ToStrView(pod.first).ToCStrView();
         rpt.Tab_ = &tree.LayoutC_.TabArray_[L];
         if (seed->SeedHandler_.FnOnReport_) {
            rpt.UserData_ = seed->SeedHandler_.UserData_;
            seed->SeedHandler_.FnOnReport_(&ses, &rpt);
         }
         if (svc::SubrRec* subr = maplk->SubrMap_.GetObjPtr(seed->SubrIndex_)) {
            if (subr->SubrHandler_.FnOnReport_) {
               rpt.UserData_ = subr->SubrHandler_.UserData_;
               subr->SubrHandler_.FnOnReport_(&ses, &rpt);
            }
            maplk->SubrMap_.RemoveObjPtr(seed->SubrIndex_, nullptr);
            seed->ClearSubscriber();
         }
      }
   }
   tree.PendingReqs_.clear();
   tree.PodMap_.clear();
}
//--------------------------------------------------------------------------//
RcSvcAckBase::~RcSvcAckBase() {
}
void RcSvcAckBase::LogResult(LogLevel lv, f9sv_Result res) const {
   if (this->IsNeedsLogResult_)
      fon9_LOG(lv, "f9sv_", this->FnName_, ".Ack|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&this->Session_)),
               this->ReqKey_, lv == LogLevel::Info ? StrView{"|res="} : StrView{"|err="},
               res, ':', f9sv_GetSvResultMessage(res));
}
void RcSvcAckBase::Run() {
   // -----
   // 從 this->RxBuf_ 載入 ReqKey_ 之後, 接下來的格式, 依序如下:
   // - if(IsBitvIntegerNeg())
   //       - f9sv_Result for 錯誤碼. tree 首次查詢的失敗, 此時沒有 layout.
   //       - ### 此次的封包到此結束 ###
   // - if(HasSvFuncFlag(fcAck, SvFuncFlag::Layout))
   //       - string for layout.
   // - if(fon9_BitvV_NumberNull) : (subr: tabCount) or (query: tabidx) or (command,write,gridview: resultCode)
   //       - not_found_tab;
   //       - 因為此時無法把 Handler 放入對應的 seed, 所以必定已從 pending 回覆失敗了.
   //       - 在已有 layout 時的查詢、訂閱, 若 tab 不存在, 則會立即觸發失敗, 不會來到此處.
   //       - 所以 ### 此次的封包到此結束 ###
   // - if(fcCode == SvFuncCode::Subscribe)
   //       - N(>=0): 訂閱時立即回覆的 tab(seeds) 數量;
   //         避免: 如果這裡失敗, 且沒有 FlagLayout, 則會與 [tree 首次查詢失敗] 造成衝突.
   //       - f9sv_Result_NoError 
   //       - 否則 f9sv_Result for 訂閱失敗原因(此時N必定==0), ### 此次的封包到此結束 ###
   // - if(fcCode == SvFuncCode::Query)
   //       - N = (IsAllTabs ? layout.TabCount : 1);
   // - 重複 N 次(每個 tab 一次):
   //       - TabIndex + [IsBitvIntegerNeg(f9sv_Result) for 錯誤碼] or [string for SeedData]
   // -----
   if (IsBitvIntegerNeg(this->RxBuf_)) {
      // TreePath 查詢失敗, 清除此 tree 全部的要求.
      // 有可能 tree 的某些 key 有權限, 所以底下的 assert() 不一定成立.
      // 但為了簡化規則, 此時依然: 清除此 tree 全部的要求.
      // assert(tree.Layout_.get() == nullptr && tree.PodMap_.empty());
      f9sv_Result res{};
      BitvTo(this->RxBuf_, res);
      this->LogResult(LogLevel::Error, res);
      RcSvcTreeRejector{res}.Run(this->TreeMapLk_, this->Session_, this->Tree_);
      return;
   }
   // -----
   if (this->HasSvFuncLayout_) {
      std::string          strLayout;
      BitvTo(this->RxBuf_, strLayout);

      StrView      cfgstr{&strLayout};
      StrTrimTail(&cfgstr);

      if (this->Session_.LogFlags_ & f9sv_ClientLogFlag_RequestMask)
         fon9_LOG_INFO("f9sv_Layout|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&this->Session_)),
                       "|path=", this->Tree_.TreePath_, "|layout=[", cfgstr, ']');
      this->Tree_.ParseLayout(cfgstr);
   }
   // -----
   bool isTabNotFound = false;
   if (const byte* tabi = this->RxBuf_.Peek1()) {
      if (*tabi == fon9_BitvV_NumberNull) { // not_found_tab
         this->RxBuf_.PopConsumed(1);
         isTabNotFound = true;
      }
   }
   else {
      Raise<BitvNeedsMore>("RcSvcAckBase::Run(): needs more");
   }
   // -----
   if (fon9_LIKELY(this->Tree_.PendingReqs_.empty())) {
      // - 已經有 Layout 的 RcSvReq, 在送出前, 已經檢查過 TabIndex, TabName,
      //   => 所以這裡必定不可能 isTabNotFound;
      // - 沒有 Layout 的 RcSvReq, 只有第一筆會送出,
      //   且在收到 Layout 前, 第一筆及其後的要求, 都會放到 PendingReqs,
      //   等收到 Layout 時透過 RcSvcReq::RunPendingReqs() 處理;
      assert(!isTabNotFound);
   }
   else {
      RcSvcReq::RunPendingReqs(this->TreeMapLk_, this->Tree_, isTabNotFound);
      if (isTabNotFound)
         return;
   }
   // -----
   f9sv_ClientReport rpt;
   ZeroStruct(rpt);
   rpt.Layout_ = &this->Tree_.LayoutC_;
   rpt.TreePath_ = ToStrView(this->Tree_.TreePath_).ToCStrView();
   rpt.SeedKey_ = ToStrView(this->ReqKey_.SeedKey_).ToCStrView();
   this->RptRun(rpt, this->Tree_.FetchPod(ToStrView(this->ReqKey_.SeedKey_)));
}
f9sv_TabSize RcSvcAckBase::ReadTabIdxAndResultCodeNoLog(f9sv_ClientReport& rpt) {
   f9sv_TabSize tabidx{kTabAll};
   BitvTo(this->RxBuf_, tabidx);
   if (tabidx == kTabAll)
      rpt.ResultCode_ = f9sv_Result_NotFoundTab;
   else
      BitvTo(this->RxBuf_, rpt.ResultCode_);
   return tabidx;
}
void RcSvcAckBase::ReadSeedsResult(f9sv_ClientReport& rpt, svc::PodRec& pod, f9sv_TabSize tabCount, SvFuncCode fcCode) {
   // ----- {讀取 tab(seeds) 結果}
   for (f9sv_TabSize L = 0; L < tabCount; ++L) {
      f9sv_TabSize  tabidx{kTabAll};
      BitvTo(this->RxBuf_, tabidx);
      if (tabidx >= this->Tree_.LayoutC_.TabCount_)
         break;
      rpt.Tab_ = &this->Tree_.LayoutC_.TabArray_[tabidx];
      svc::SeedRec* seed = pod.Seeds_[tabidx].get();
      assert(seed != nullptr);
      f9sv_FnOnReport fnOnReport;
      if (fcCode == SvFuncCode::Subscribe) {
         auto*  subr = this->TreeMapLk_->SubrMap_.GetObjPtr(seed->SubrIndex_);
         assert(subr != nullptr);
         fnOnReport = subr->SubrHandler_.FnOnReport_;
         rpt.UserData_ = subr->SubrHandler_.UserData_;
      }
      else {
         fnOnReport = seed->OnBeforeReportToUser(rpt, *seed, fcCode);
      }
      if (IsBitvIntegerNeg(this->RxBuf_)) {
         // 查詢失敗. (訂閱失敗不會來到 tab(seed) 這裡?).
         BitvTo(this->RxBuf_, rpt.ResultCode_);
         this->LogResult(LogLevel::Error, rpt.ResultCode_);
         if (fnOnReport) {
            rpt.Seed_ = nullptr;
            fnOnReport(&this->Session_, &rpt);
         }
      }
      else {
         // 查詢成功 or 訂閱成功的立即回覆.
         rpt.Seed_ = seed;
         std::string fldValues;
         BitvTo(this->RxBuf_, fldValues);
         this->ReportSeedValues(fnOnReport, rpt, &fldValues);
      }
   }
}
void RcSvcAckBase::ReportSeedValues(f9sv_FnOnReport fnOnReport, f9sv_ClientReport& rpt, StrView fldValues) {
   const auto* tab = this->Tree_.Layout_->GetTab(unsigned_cast(rpt.Tab_->Named_.Index_));
   if (fon9_UNLIKELY(this->IsNeedsLogData_ && LogLevel::Info >= LogLevel_)) {
      RevBufferList rbuf_{fon9::kLogBlockNodeSize};
      RevPrint(rbuf_, '\n'); // 自己呼叫 LogWrite() 則需要自行加上 '\n';
      if (rpt.ExResult_.Begin_ != rpt.ExResult_.End_)
         RevPrint(rbuf_, "|exMsg={", rpt.ExResult_, '}');
      RevPrint(rbuf_, "|gv={", fldValues, '}');
      if (this->ReqKey_.IsAllTabs_ || this->ReqKey_.TabName_.empty())
         RevPrint(rbuf_, ':', tab->Name_, ':', tab->GetIndex());
      RevPrint(rbuf_, "f9sv_", this->FnName_, ".Data|userData=", ToPtr(rpt.UserData_), this->ReqKey_);
      LogWrite(LogLevel::Info, std::move(rbuf_));
   }
   if (fnOnReport) {
      RcSvcNotifySeeds(fnOnReport, &this->Session_, &rpt, fldValues,
                       seed::SimpleRawWr{const_cast<struct f9sv_Seed*>(rpt.Seed_)}, tab->Fields_,
                       seed::IsSubrTree(this->ReqKey_.SeedKey_.begin()));
   }
}
void RcSvcAckBase::ReportToUser(f9sv_ClientReport& rpt, svc::PodRec& pod, f9sv_TabSize tabidx, SvFuncCode fcCode, bool isSetRptSeed) const {
   if (svc::SeedRec* seed = pod.Seeds_[tabidx].get())
      if (auto fnOnReport = seed->OnBeforeReportToUser(rpt, *seed, fcCode)) {
         rpt.Tab_ = &this->Tree_.LayoutC_.TabArray_[tabidx];
         // rpt.Seed_ = seed; 某些功能(例: Remove), 不論成功或失敗, 都不應提供 rpt.Seed_;
         rpt.Seed_ = isSetRptSeed ? seed : nullptr;
         fnOnReport(&this->Session_, &rpt);
      }
}
//--------------------------------------------------------------------------//
void RcSvcAckSubscribe::RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) {
   // {取得訂閱結果}
   // 有多少立即回覆的 tab(seeds) 數量?
   // 組裝結果在 RcSeedVisitorServer.cpp: RcSeedVisitorServerNote::OnSubscribeNotify();
   f9sv_TabSize  tabCount = 0;
   BitvTo(this->RxBuf_, tabCount);
   BitvTo(this->RxBuf_, rpt.ResultCode_);
   this->LogResult(rpt.ResultCode_);

   auto* subr = this->TreeMapLk_->SubrMap_.GetObjPtr(this->ReqKey_.SubrIndex_);
   assert(!this->ReqKey_.IsAllTabs_ && subr != nullptr && subr->TabIndex_ != kTabAll);

   svc::SeedRec* seed = pod.Seeds_[subr->TabIndex_].get();
   rpt.Tab_ = &this->Tree_.LayoutC_.TabArray_[subr->TabIndex_];

   auto fnOnReport = subr->SubrHandler_.FnOnReport_;
   if (rpt.ResultCode_ != f9sv_Result_NoError) {
      seed->ClearSubscriber();
      subr->ClearSvFuncSubr();
   }
   if (fnOnReport) {
      // 訂閱失敗, 即使送出取消註冊, 也不會收到回覆, 所以在此必須通知.
      // 訂閱成功:
      // - 仍在訂閱, 才需要通知.
      // - 否則應該正在取消訂閱, 需要等取消訂閱結果, 然後才通知.
      if (rpt.ResultCode_ != f9sv_Result_NoError
          || subr->SubrSvFunc_ == SvFuncCode::Subscribe) {
         rpt.Seed_ = nullptr;
         rpt.UserData_ = subr->SubrHandler_.UserData_;
         fnOnReport(&this->Session_, &rpt);
      }
   }
   if (rpt.ResultCode_ != f9sv_Result_NoError) {
      this->TreeMapLk_->SubrMap_.RemoveObjPtr(this->ReqKey_.SubrIndex_, subr);
      return;
   }
   if (subr->IsStream_) {
      svc::TabRec& rtab = this->Tree_.TabArray_[subr->TabIndex_];
      if (rtab.StreamDecoder_.get() == nullptr) {
         auto*  fac = RcSvStreamDecoderPark::Register(RcSvcGetStreamDecoderName(ToStrView(this->ReqKey_.TabName_)), nullptr);
         assert(fac != nullptr); // 因為在訂閱前已經確認過, 此時一定可以取得 fac.
         rtab.StreamDecoder_ = std::move(fac->CreateStreamDecoder(this->Tree_));
         assert(rtab.StreamDecoder_.get() != nullptr);
      }
      if (seed->StreamDecoderNote_.get() == nullptr) {
         seed->StreamDecoderNote_ = std::move(rtab.StreamDecoder_->CreateDecoderNote());
         assert(seed->StreamDecoderNote_.get() != nullptr);
      }
      assert(tabCount == 0);
      std::string ack;
      BitvTo(this->RxBuf_, ack);
      rpt.ResultCode_ = f9sv_Result_SubrStreamOK;
      rtab.StreamDecoder_->OnSubscribeStreamOK(*subr, &ack, this->Session_, rpt, this->IsNeedsLogResult_);
   }
   else {
      this->ReadSeedsResult(rpt, pod, tabCount, SvFuncCode::Subscribe);
   }
}
//--------------------------------------------------------------------------//
void RcSvcAckQuery::RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) {
   this->ReadSeedsResult(rpt, pod,
                         this->ReqKey_.IsAllTabs_ ? this->Tree_.LayoutC_.TabCount_ : 1,
                         SvFuncCode::Query);
}
//--------------------------------------------------------------------------//
void RcSvcAckGridView::RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) {
   f9sv_TabSize tabidx = this->ReadTabIdxAndResultCodeAndLog(rpt);
   if (tabidx == kTabAll)
      return;
   rpt.GridViewTableSize_ = rpt.GridViewDistanceBegin_ = rpt.GridViewDistanceEnd_ = f9sv_ROWNO_NOT_SUPPORT;
   std::string  strGvResult;
   if (rpt.ResultCode_ == f9sv_Result_NoError) {
      BitvTo(this->RxBuf_, rpt.GridViewResultCount_);
      BitvTo(this->RxBuf_, rpt.GridViewTableSize_);
      BitvTo(this->RxBuf_, rpt.GridViewDistanceBegin_);
      BitvTo(this->RxBuf_, rpt.GridViewDistanceEnd_);
      BitvTo(this->RxBuf_, strGvResult);
      if (this->IsNeedsLogResult_)
         fon9_LOG_INFO("Rows=", rpt.GridViewResultCount_, '/', signed_cast(rpt.GridViewTableSize_),
                       "|Dist=", signed_cast(rpt.GridViewDistanceBegin_), ':', signed_cast(rpt.GridViewDistanceEnd_),
                       "|gv=\n", strGvResult);
   }
   assert(tabidx < this->Tree_.LayoutC_.TabCount_);
   if (tabidx < this->Tree_.LayoutC_.TabCount_) {
      rpt.ExResult_ = ToStrView(strGvResult).ToCStrView();
      this->ReportToUser(rpt, pod, tabidx, SvFuncCode::GridView, (rpt.ResultCode_ == f9sv_Result_NoError));
   }
}
//--------------------------------------------------------------------------//
void RcSvcAckRemove::RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) {
   BitvTo(this->RxBuf_, rpt.ResultCode_); // f9sv_Result_RemovePod or f9sv_Result_RemoveSeed;
   this->LogResult(rpt.ResultCode_);
   f9sv_TabSize tabCount = this->Tree_.LayoutC_.TabCount_;
   if (rpt.ResultCode_ == f9sv_Result_RemoveSeed) {
      // 針對指定的 seed 回報.
      f9sv_TabSize tabidx{kTabAll};
      BitvTo(this->RxBuf_, tabidx);
      assert(tabidx < tabCount);
      if (tabidx < tabCount)
         this->ReportToUser(rpt, pod, tabidx, SvFuncCode::Remove, false);
   }
   else { // pod 裡面全部的 seed 都要回報.
      for (f9sv_TabSize tabidx = 0; tabidx < tabCount; ++tabidx)
         this->ReportToUser(rpt, pod, tabidx, SvFuncCode::Remove, false);
   }
}
//--------------------------------------------------------------------------//
void RcSvcAckWrite::RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) {
   f9sv_TabSize tabidx = this->ReadTabIdxAndResultCodeAndLog(rpt);
   if (tabidx == kTabAll)
      return;
   if (rpt.ResultCode_ != f9sv_Result_NoError)
      this->ReportToUser(rpt, pod, tabidx, SvFuncCode::Write, false);
   else {
      std::string  fldValues;
      std::string  msgResult;
      BitvTo(this->RxBuf_, fldValues);
      BitvTo(this->RxBuf_, msgResult);
      assert(tabidx < this->Tree_.LayoutC_.TabCount_);
      if (tabidx < this->Tree_.LayoutC_.TabCount_) {
         if (svc::SeedRec* seed = pod.Seeds_[tabidx].get())
            if (auto fnOnReport = seed->OnBeforeReportToUser(rpt, *seed, SvFuncCode::Write)) {
               rpt.Tab_ = &this->Tree_.LayoutC_.TabArray_[tabidx];
               rpt.Seed_ = seed;
               // 移除最後換行字元: log 會比較整齊.
               if (!msgResult.empty() && msgResult.back() == '\n')
                  msgResult.pop_back();
               rpt.ExResult_ = ToStrView(msgResult).ToCStrView();
               this->ReportSeedValues(fnOnReport, rpt, &fldValues);
            }
      }
   }
}
//--------------------------------------------------------------------------//
void RcSvcAckCommand::RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) {
   f9sv_TabSize tabidx = this->ReadTabIdxAndResultCodeNoLog(rpt);
   if (tabidx == kTabAll) {
      this->LogResult(rpt.ResultCode_);
      return;
   }
   std::string  strCmdResult;
   if (rpt.ResultCode_ == f9sv_Result_NoError) {
      if (!IsBitvByteArray(this->RxBuf_))
         BitvTo(this->RxBuf_, rpt.ResultCode_);
      BitvTo(this->RxBuf_, strCmdResult);
      rpt.ExResult_ = ToStrView(strCmdResult).ToCStrView();
   }
   if (this->IsNeedsLogResult_) {
      this->LogResult(rpt.ResultCode_);
      fon9_LOG_INFO("tabidx=", tabidx, "|cmd.result={", strCmdResult, '}');
   }
   this->ReportToUser(rpt, pod, tabidx, SvFuncCode::Command, false);
}

} } // namespaces
