// \file fon9/rc/RcClientSession.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcClientSession.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace rc {

RcClientSession::RcClientSession(RcFunctionMgrSP funcMgr, const f9rc_ClientSessionParams* params)
   : base(std::move(funcMgr), RcSessionRole::User, params->RcFlags_)
   , Password_{params->Password_}
   , FnOnLinkEv_(params->FnOnLinkEv_) {
   this->SetUserId(StrView_cstr(params->UserId_));
   this->UserData_ = params->UserData_;
   this->LogFlags_ = params->LogFlags_;
   for (unsigned L = 0; L < f9rc_FunctionCode_Count; ++L) {
      if (auto agent = dynamic_cast<RcClientFunctionAgent*>(this->FunctionMgr_->Get(static_cast<f9rc_FunctionCode>(L))))
         agent->OnSessionCtor(*this, params);
   }
}
StrView RcClientSession::GetAuthPassword() const {
   return StrView{&this->Password_};
}
//--------------------------------------------------------------------------//
RcClientFunctionAgent::~RcClientFunctionAgent() {
}
void RcClientFunctionAgent::OnSessionCtor(RcClientSession&, const f9rc_ClientSessionParams*) {
}
//--------------------------------------------------------------------------//
RcClientMgr::RcClientSessionFactory::RcClientSessionFactory() : SessionFactory{"RcClientSession"} {
}
io::SessionSP RcClientMgr::RcClientSessionFactory::CreateSession(IoManager& mgr, const IoConfigItem& iocfg, std::string& errReason) {
   (void)errReason;
   return new RcClientSession(static_cast<RcClientMgr*>(&mgr), static_cast<const RcClientConfigItem*>(&iocfg)->Params_);
}
io::SessionServerSP RcClientMgr::RcClientSessionFactory::CreateSessionServer(IoManager&, const IoConfigItem&, std::string& errReason) {
   errReason = "RcClientSession: Not support server.";
   return nullptr;
}
//--------------------------------------------------------------------------//
unsigned RcClientMgr::IoManagerAddRef() { return intrusive_ptr_add_ref(this); }
unsigned RcClientMgr::IoManagerRelease() { return intrusive_ptr_release(this); }
void RcClientMgr::NotifyChanged(IoManager::DeviceRun&) {}
void RcClientMgr::NotifyChanged(IoManager::DeviceItem&) {}

RcClientMgr::RcClientMgr(const IoManagerArgs& ioMgrArgs) : base{ioMgrArgs} {
   this->DeviceFactoryPark_->Add(MakeIoFactoryTcpClient("TcpClient"));
   this->Add(RcFunctionAgentSP{new RcFuncConnClient("f9rcAPI.0", "f9rc RcClient API")});
   this->Add(RcFunctionAgentSP{new RcFuncSaslClient{}});
}

int RcClientMgr::CreateRcClientSession(f9rc_ClientSession** result, const RcClientConfigItem& cfg) {
   auto fac = this->DeviceFactoryPark_->Get(StrView_cstr(cfg.Params_->DevName_));
   if (!fac)
      return false;
   std::string errReason;
   auto dev = fac->CreateDevice(this, this->RcClientSessionFactory_, cfg, errReason);
   if (result)
      *result = static_cast<RcClientSession*>(dev->Session_.get());
   dev->Initialize();
   dev->AsyncOpen(cfg.Params_->DevParams_);
   dev.detach(); // 配合在 this->DestroyRcClientSession() 裡面的 dev 使用 add_ref = false;
   return true;
}
void RcClientMgr::DestroyRcClientSession(RcClientSession* ses, int isWait) {
   // add_ref = false: 配合在 CreateRcClientSession(); 裡面有呼叫 dev.detach();
   io::DeviceSP dev(ses->GetDevice(), false);
   dev->AsyncDispose("DestroyRcClientSession");
   dev->WaitGetDeviceId();
   if (isWait) {
      while (dev->use_count() > 1)
         std::this_thread::yield();
   }
}

void RcClientMgr::OnDevice_Destructing(io::Device&) {
}
void RcClientMgr::OnDevice_Accepted(io::DeviceServer&, io::DeviceAcceptedClient&) {
}
void RcClientMgr::OnDevice_Initialized(io::Device&) {
}

static void LogDevSt(StrView head, io::Device& dev, const io::StateUpdatedArgs& e) {
   assert(dynamic_cast<RcClientSession*>(dev.Session_.get()) != nullptr);
   auto* ses = static_cast<RcClientSession*>(dev.Session_.get());
   if (ses->LogFlags_ & f9rc_ClientLogFlag_Link)
      fon9_LOG_INFO(head,
                    "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                    "|dev=", ToPtr(&dev),
                    "|st=", io::GetStateStr(e.State_),
                    "|devid=", e.DeviceId_,
                    "|info=", e.Info_);
   if (ses->FnOnLinkEv_)
      ses->FnOnLinkEv_(ses, static_cast<f9io_State>(e.State_), e.Info_.ToCStrView());
}
void RcClientMgr::OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) {
   LogDevSt("RcClient.OnDeviceSt", dev, e.After_);
}
void RcClientMgr::OnDevice_StateUpdated(io::Device& dev, const io::StateUpdatedArgs& e) {
   LogDevSt("RcClient.OnDeviceStU", dev, e);
}
void RcClientMgr::OnSession_StateUpdated(io::Device& dev, StrView stmsg, LogLevel) {
   (void)stmsg;
   assert(dynamic_cast<RcClientSession*>(dev.Session_.get()) != nullptr);
   auto* ses = static_cast<RcClientSession*>(dev.Session_.get());
   if (ses->GetSessionSt() == RcSessionSt::ApReady && ses->LogFlags_ & f9rc_ClientLogFlag_Link)
      fon9_LOG_INFO("RcClient.ApReady"
                    "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(ses)),
                    "|dev=", ToPtr(ses->GetDevice()),
                    stmsg);
}

} } // namespace
