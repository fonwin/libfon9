// \file fon9/rc/RcSeedVisitorClient.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorClient_hpp__
#define __fon9_rc_RcSeedVisitorClient_hpp__
#include "fon9/rc/RcClientSession.hpp"
#include "fon9/rc/RcSvStreamDecoder.hpp"

namespace fon9 { namespace rc {

/// \ingroup rc
/// FunctionAgent: SeedVisitor Client.
/// ApReady 後, 送出權限查詢訊息, 取得可 tree list 的相關權限.
class RcSeedVisitorClientAgent : public RcClientFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcSeedVisitorClientAgent);
   using base = RcClientFunctionAgent;
public:
   RcSeedVisitorClientAgent() : base{f9rc_FunctionCode_SeedVisitor} {
   }
   ~RcSeedVisitorClientAgent();

   void OnSessionCtor(RcClientSession& ses, const f9rc_ClientSessionParams* params);
   void OnSessionApReady(RcSession& ses) override;
   void OnSessionLinkBroken(RcSession& ses) override;
};

struct RcSvClientRequest;

/// - 在 Rc Session 建立時, 就會將 RcSeedVisitorClientNote 建立好.
/// - 在 ApReady/LinkBroken 時, 會呼叫 this->ClearRequests() 清除全部的訂閱及Layout.
class RcSeedVisitorClientNote : public RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(RcSeedVisitorClientNote);
public:
   const f9sv_ClientSessionParams   Params_;

   RcSeedVisitorClientNote(const f9sv_ClientSessionParams& params) : Params_(params) {
   }
   ~RcSeedVisitorClientNote();

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;

   /// 在 RcSeedVisitorClientAgent::OnSessionLinkBroken() 轉呼叫此處.
   /// - 在此呼叫 this->ClearRequests(): 不保留訂閱設定, 重新連線後, 由使用者自行重新訂閱.
   void OnSessionLinkBroken(RcClientSession& ses);

   /// - 清除全部的訂閱及Layout;
   /// - 呼叫時機: 在 ApReady 及 LinkBroken 時.
   void ClearRequests(RcClientSession& ses);

   using TreeLocker = svc::TreeLocker;
   TreeLocker LockTree() {
      return this->TreeMap_.Lock();
   }

   /// 檢查「sv 查詢要求」的流量管制.
   TimeInterval FcQryFetch(const TreeLocker& locker) {
      (void)locker; assert(locker.owns_lock());
      return this->FcQry_.Fetch();
   }

   f9sv_Result AddSubr(const TreeLocker& locker, RcSvClientRequest& req, const svc::PodRec* pod);

private:
   struct RejectRequest;

   void OnRecvAclConfig(RcClientSession& ses, RcFunctionParam& param);
   void OnRecvQrySubrAck(RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck, StrView fnName);
   void OnRecvUnsubrAck(RcClientSession& ses, DcQueue& rxbuf);
   void OnRecvSubrData(RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck);
   void SendPendings(const TreeLocker& maplk, RcClientSession& ses, svc::TreeRec& tree, bool isTabNotFound);

   // ConfigGvTablesStr_: 提供給 fon9::rc::SvParseGvTables() 解析使用的訊息, 分隔符號變成 EOS.
   std::string          ConfigGvTablesStr_;
   SvGvTables           ConfigGvTables_;
   f9sv_ClientConfig    Config_;
   std::string          OrigGvTablesStr_;
   seed::AclConfig      Ac_;
   FlowCounter          FcQry_;
   svc::TreeMap         TreeMap_;
};

} } // namespaces
#endif//__fon9_rc_RcSeedVisitorClient_hpp__
