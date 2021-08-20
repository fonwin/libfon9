// \file fon9/rc/RcSvcAck.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSvcAck_hpp__
#define __fon9_rc_RcSvcAck_hpp__
#include "fon9/rc/RcClientSession.hpp"
#include "fon9/rc/RcSvStreamDecoder.hpp"

namespace fon9 { namespace rc {

/// 查詢結果: 使用[文字]傳輸 seed 內容:
/// 組裝結果在 RcSeedVisitorServer.cpp: RcSvTicketRunner::PackSeedValues();
void RcSvcNotifySeeds(f9sv_FnOnReport fnOnReport,
                      f9rc_ClientSession* ses,
                      f9sv_ClientReport* rpt,
                      StrView gv,
                      const seed::RawWr& wr,
                      const seed::Fields& flds,
                      bool hasKeyText);
//--------------------------------------------------------------------------//
struct RcSvcTreeRejector {
   f9sv_ClientReport rpt;
   f9sv_Tab          tab;
   RcSvcTreeRejector(f9sv_Result res) {
      ZeroStruct(rpt);
      ZeroStruct(tab);
      rpt.ResultCode_ = res;
   }
   void Run(svc::TreeLocker& maplk, RcClientSession& ses, svc::TreeRec& tree);
};
//--------------------------------------------------------------------------//
struct RcSvcAckBase {
   fon9_NON_COPY_NON_MOVE(RcSvcAckBase);
   const RcSvReqKey     ReqKey_;
   DcQueue&             RxBuf_;
   const StrView        FnName_;
   const bool           IsNeedsLogResult_;
   const bool           IsNeedsLogData_;
   const bool           HasSvFuncLayout_;
   char                 Padding__[5];
   RcClientSession&     Session_;
   svc::TreeMap::Locker TreeMapLk_;
   svc::TreeRec&        Tree_;

   RcSvcAckBase(svc::TreeMap& treeMap, RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck, StrView fnName,
                f9rc_ClientLogFlag logResultFlag, f9rc_ClientLogFlag logDataFlag)
      : ReqKey_{fcAck, rxbuf}
      , RxBuf_(rxbuf)
      , FnName_{fnName}
      , IsNeedsLogResult_{(ses.LogFlags_ & logResultFlag) != f9rc_ClientLogFlag_None}
      , IsNeedsLogData_{(ses.LogFlags_ & logDataFlag) != f9rc_ClientLogFlag_None}
      , HasSvFuncLayout_(HasSvFuncFlag(fcAck, SvFuncFlag::Layout))
      , Session_(ses)
      , TreeMapLk_{treeMap}
      , Tree_(TreeMapLk_->fetch(ToStrView(ReqKey_.TreePath_))) {
   }

   virtual ~RcSvcAckBase();

   void Run();

protected:
   void LogResult(LogLevel lv, f9sv_Result res) const;
   void LogResult(f9sv_Result res) const {
      this->LogResult((res < f9sv_Result_NoError ? LogLevel::Error : LogLevel::Info), res);
   }

   virtual void RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) = 0;

   /// 載入 tabidx 及 rpt.ResultCode_;
   f9sv_TabSize ReadTabIdxAndResultCodeAndLog(f9sv_ClientReport& rpt) {
      auto retval = this->ReadTabIdxAndResultCodeNoLog(rpt);
      this->LogResult(rpt.ResultCode_);
      return retval;
   }
   f9sv_TabSize ReadTabIdxAndResultCodeNoLog(f9sv_ClientReport& rpt);

   void ReadSeedsResult(f9sv_ClientReport& rpt, svc::PodRec& pod, f9sv_TabSize tabCount, SvFuncCode fcCode);
   void ReportSeedValues(f9sv_FnOnReport fnOnReport, f9sv_ClientReport& rpt, StrView fldValues);
   void ReportToUser(f9sv_ClientReport& rpt, svc::PodRec& pod, f9sv_TabSize tabidx, SvFuncCode fcCode, bool isSetRptSeed) const;
};
//--------------------------------------------------------------------------//
struct RcSvcAckSubscribe : public RcSvcAckBase {
   fon9_NON_COPY_NON_MOVE(RcSvcAckSubscribe);
   RcSvcAckSubscribe(svc::TreeMap& treeMap, RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck)
      : RcSvcAckBase(treeMap, ses, rxbuf, fcAck, "Subscribe", f9sv_ClientLogFlag_Subscribe, f9sv_ClientLogFlag_SubscribeData) {
   }
   void RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) override;
};
//--------------------------------------------------------------------------//
struct RcSvcAckQuery : public RcSvcAckBase {
   fon9_NON_COPY_NON_MOVE(RcSvcAckQuery);
   RcSvcAckQuery(svc::TreeMap& treeMap, RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck)
      : RcSvcAckBase(treeMap, ses, rxbuf, fcAck, "Query", f9sv_ClientLogFlag_Query, f9sv_ClientLogFlag_QueryData) {
   }
   void RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) override;
};
//--------------------------------------------------------------------------//
struct RcSvcAckGridView : public RcSvcAckBase {
   fon9_NON_COPY_NON_MOVE(RcSvcAckGridView);
   RcSvcAckGridView(svc::TreeMap& treeMap, RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck)
      : RcSvcAckBase(treeMap, ses, rxbuf, fcAck, "GridView", f9sv_ClientLogFlag_GridView, f9rc_ClientLogFlag_None) {
   }
   void RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) override;
};
//--------------------------------------------------------------------------//
struct RcSvcAckRemove : public RcSvcAckBase {
   fon9_NON_COPY_NON_MOVE(RcSvcAckRemove);
   RcSvcAckRemove(svc::TreeMap& treeMap, RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck)
      : RcSvcAckBase(treeMap, ses, rxbuf, fcAck, "Remove", f9sv_ClientLogFlag_Remove, f9rc_ClientLogFlag_None) {
   }
   void RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) override;
};
//--------------------------------------------------------------------------//
struct RcSvcAckWrite : public RcSvcAckBase {
   fon9_NON_COPY_NON_MOVE(RcSvcAckWrite);
   RcSvcAckWrite(svc::TreeMap& treeMap, RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck)
      : RcSvcAckBase(treeMap, ses, rxbuf, fcAck, "Write", f9sv_ClientLogFlag_Write, f9sv_ClientLogFlag_Write) {
   }
   void RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) override;
};
//--------------------------------------------------------------------------//
struct RcSvcAckCommand : public RcSvcAckBase {
   fon9_NON_COPY_NON_MOVE(RcSvcAckCommand);
   RcSvcAckCommand(svc::TreeMap& treeMap, RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck)
      : RcSvcAckBase(treeMap, ses, rxbuf, fcAck, "Command", f9sv_ClientLogFlag_Command, f9rc_ClientLogFlag_None) {
   }
   void RptRun(f9sv_ClientReport& rpt, svc::PodRec& pod) override;
};

} } // namespaces
#endif//__fon9_rc_RcSvcAck_hpp__
