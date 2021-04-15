// \file fon9/rc/RcSeedVisitorServer.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorServer_hpp__
#define __fon9_rc_RcSeedVisitorServer_hpp__
#include "fon9/rc/Rc.hpp"
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/io/Device.hpp"
#include "fon9/seed/SeedVisitor.hpp"
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/FlowControlCalc.hpp"

namespace fon9 { namespace rc {

class fon9_API RcSeedVisitorServerNote;

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

   struct UnsubRunnerSP;
   struct TicketRunnerSubscribe;
   /// 在呼叫 op.Subscribe() 之前設定, 在 NotifyKind = SubscribeOK 時, 送出訂閱結果.
   /// 此時 SubrList_ 必定在鎖定狀態.
   TicketRunnerSubscribe* Runner_{};
   struct SubrReg : public intrusive_ref_counter<SubrReg> {
      fon9_NON_COPY_NON_MOVE(SubrReg);
      SubrReg(f9sv_SubrIndex subrIndex, StrView seedKey, io::DeviceSP dev)
         : SubrIndex_{subrIndex}
         , SeedKey_{seedKey}
         , Device_{std::move(dev)} {
      }
      seed::OpResult Unsubscribe(seed::SubscribableOp* opSubr, seed::Tab& tab) {
         if (opSubr) {
            if (this->IsStream_)
               return opSubr->UnsubscribeStream(&this->SubConn_, tab);
            return opSubr->Unsubscribe(&this->SubConn_, tab);
         }
         return seed::OpResult::no_subscribe;
      }
      const f9sv_SubrIndex SubrIndex_;
      f9sv_TabSize         TabIndex_;
      SvFuncCode           SvFunc_{SvFuncCode::Subscribe};
      bool                 IsStream_{false};
      /// 當收到 NotifyKind = ParentSeedClear, 或 訂閱 seed 收到 PodRemoved, SeedRemoved 時;
      bool                 IsSubjectClosed_{false};
      char                 Padding___[1];
      seed::TreeSP         Tree_;
      SubConn              SubConn_{};
      const CharVector     SeedKey_;
      const io::DeviceSP   Device_;
   };
   using SubrRegSP = intrusive_ptr<SubrReg>;
   using SubrListImpl = std::vector<SubrRegSP>; // 索引值為 Client 端的 SubrIndex;
   using SubrList = MustLock<SubrListImpl>;
   SubrList SubrList_;

   static void OnSubscribeNotify(SubrRegSP preg, const seed::SeedNotifyArgs& e);
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
