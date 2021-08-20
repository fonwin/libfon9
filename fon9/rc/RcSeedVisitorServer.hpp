// \file fon9/rc/RcSeedVisitorServer.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorServer_hpp__
#define __fon9_rc_RcSeedVisitorServer_hpp__
#include "fon9/rc/Rc.hpp"
#include "fon9/rc/RcSvsTicket.hpp"
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/FlowControlCalc.hpp"

namespace fon9 { namespace rc {

/// \ingroup rc
/// FunctionAgent: SeedVisitor Server.
class fon9_API RcSeedVisitorServerAgent : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcSeedVisitorServerAgent);
   using base = RcFunctionAgent;
protected:
   virtual RcFunctionNoteSP OnCreateRcSeedVisitorServerNote(RcSession& ses,
                                                            const auth::AuthResult& authr,
                                                            seed::AclConfig&& aclcfg);
public:
   RcSeedVisitorServerAgent() : base{f9rc_FunctionCode_SeedVisitor} {
   }
   ~RcSeedVisitorServerAgent();

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;
   void OnSessionLinkBroken(RcSession& ses) override;
};

class fon9_API RcSeedVisitorServerNote : public RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(RcSeedVisitorServerNote);
   struct SeedVisitor;
   using SeedVisitorSP = intrusive_ptr<SeedVisitor>;
   FlowCounter       FcQry_;
   FlowControlCalc   FcRecover_;
   SeedVisitorSP     Visitor_;
   uint32_t          FcOverCount_{};
   char              Padding____[4];

   using SubrListImpl = std::vector<RcSvsSubrSP>; // 索引值為 Client 端的 SubrIndex;
   using SubrList = MustLock<SubrListImpl>;
   SubrList SubrList_;

   friend struct RcSvsUnsubRunnerSP;
   friend struct RcSvsSubscribeRunner;
   friend struct RcSvsReq;

   /// 在呼叫 op.Subscribe() 之前設定 this->Runner_,
   /// 在 NotifyKind = SubscribeOK or SubscribeStreamOK 時,
   /// 透過 this->Runner_->AckBuffer_ 送出訂閱結果.
   /// 此時 SubrList_ 必定在鎖定狀態.
   /// 其餘不可使用 this->Runner_;
   /// 這樣可以讓忙碌的 StreamData 少傳一個參數.
   RcSvsSubscribeRunner* Runner_{};
   static void OnSubscribeNotify(RcSvsSubrSP preg, const seed::SeedNotifyArgs& e);

   void OnRecvUnsubscribe(RcSession& ses, f9sv_SubrIndex usidx);

public:
   RcSeedVisitorServerNote(RcSession& ses,
                           const auth::AuthResult& authr,
                           seed::AclConfig&& aclcfg);
   ~RcSeedVisitorServerNote();
   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;
   void OnSessionLinkBroken();
};

} } // namespaces
#endif//__fon9_rc_RcSeedVisitorServer_hpp__
