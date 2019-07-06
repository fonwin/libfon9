// \file fon9/rc/Rc.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/Rc.hpp"
#include "fon9/rc/RcSession.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace rc {

class RcFuncLogout : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFuncLogout);
   using base = RcFunctionAgent;
public:
   RcFuncLogout() : base{RcFunctionCode::Logout, RcSessionSt::ProtocolReady} {
   }
   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override {
      std::string reason{"RxLogout:"};
      BitvToStrAppend(param.RecvBuffer_, reason);
      ses.OnLogoutReceived(std::move(reason));
   }
};
class RcFuncHeartbeat : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFuncHeartbeat);
   using base = RcFunctionAgent;
public:
   RcFuncHeartbeat() : base{RcFunctionCode::Heartbeat} {
   }
   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override {
      if (IsEnumContains(ses.Role_, RcSessionRole::ProtocolInitiator))
         param.RemoveRemainParam();
      else { // Ack hb: UtcNow + OrigHbMsg
         RevBufferList rbuf{64};
         char*         pout = rbuf.AllocPrefix(param.RemainParamSize_);
         auto          rdsz = param.RecvBuffer_.Read(pout -= param.RemainParamSize_, param.RemainParamSize_);
         (void)rdsz; assert(rdsz == param.RemainParamSize_);
         rbuf.SetPrefixUsed(pout);
         ToBitv(rbuf, param.RecvTime_);
         ses.Send(RcFunctionCode::Heartbeat, std::move(rbuf));
      }
   }
};
//--------------------------------------------------------------------------//
RcFunctionMgr::~RcFunctionMgr() {
}
void RcFunctionMgr::AddDefaultAgents() {
   this->Add(RcFunctionAgentSP{new RcFuncLogout});
   this->Add(RcFunctionAgentSP{new RcFuncHeartbeat});
}
bool RcFunctionMgr::Add(RcFunctionAgentSP&& agent) {
   const size_t fnCode = static_cast<size_t>(agent->FunctionCode_);
   if (fnCode >= this->FunctionAgents_.size())
      return false;
   if (this->FunctionAgents_[fnCode])
      return false;
   this->FunctionAgents_[fnCode] = std::move(agent);
   return true;
}
void RcFunctionMgr::Reset(RcFunctionAgentSP&& agent) {
   const size_t fnCode = static_cast<size_t>(agent->FunctionCode_);
   if (fnCode < this->FunctionAgents_.size())
      this->FunctionAgents_[fnCode] = std::move(agent);
}
void RcFunctionMgr::OnSessionApReady(RcSession& ses) {
   for (const RcFunctionAgentSP& agent : this->FunctionAgents_) {
      if (agent)
         agent->OnSessionApReady(ses);
   }
}
void RcFunctionMgr::OnSessionLinkReady(RcSession& ses) {
   if (RcFuncConnClient* agent = dynamic_cast<RcFuncConnClient*>(this->Get(RcFunctionCode::Connection)))
      agent->OnSessionLinkReady(ses);
}
void RcFunctionMgr::OnSessionProtocolReady(RcSession& ses) {
   if (RcFuncConnClient* agent = dynamic_cast<RcFuncConnClient*>(this->Get(RcFunctionCode::Connection)))
      agent->OnSessionProtocolReady(ses);
}
void RcFunctionMgr::OnSessionLinkBroken(RcSession& ses) {
   for (const RcFunctionAgentSP& agent : this->FunctionAgents_) {
      if (agent)
         agent->OnSessionLinkBroken(ses);
   }
}
//--------------------------------------------------------------------------//
unsigned RcFunctionMgrRefCounter::AddRef() {
   return intrusive_ptr_add_ref(static_cast<intrusive_ref_counter<RcFunctionMgrRefCounter>*>(this));
}
unsigned RcFunctionMgrRefCounter::Release() {
   return intrusive_ptr_release(static_cast<intrusive_ref_counter<RcFunctionMgrRefCounter>*>(this));
}
//--------------------------------------------------------------------------//
RcFunctionAgent::~RcFunctionAgent() {
}
void RcFunctionAgent::OnSessionApReady(RcSession&) {
}
void RcFunctionAgent::OnSessionLinkBroken(RcSession& ses) {
   ses.ResetNote(this->FunctionCode_, nullptr);
}
void RcFunctionAgent::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   (void)ses; (void)param;
}
//--------------------------------------------------------------------------//
RcFunctionNote::~RcFunctionNote() {
}

} } // namespace
