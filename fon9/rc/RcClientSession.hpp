// \file fon9/rc/RcClientSession.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcClientSession_hpp__
#define __fon9_rc_RcClientSession_hpp__
#include "fon9/rc/RcSession.hpp"
#include "fon9/rc/RcClientApi.h"
#include "fon9/framework/IoManager.hpp"

namespace fon9 { namespace rc {

class fon9_API RcClientSession : public RcSession, public f9rc_ClientSession {
   fon9_NON_COPY_NON_MOVE(RcClientSession);
   using base = RcSession;
   std::string Password_;
public:
   const f9rc_FnOnLinkEv   FnOnLinkEv_;

   RcClientSession(RcFunctionMgrSP funcMgr, const f9rc_ClientSessionParams* params);
   StrView GetAuthPassword() const override;
};
//--------------------------------------------------------------------------//
class fon9_API RcClientFunctionAgent : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcClientFunctionAgent);
   using base = RcFunctionAgent;
public:
   using base::base;
   ~RcClientFunctionAgent();

   /// 在 RcClientSession 建構時通知, 此時「RcClientSessio的衍生者」尚未建構完畢.
   /// - 預設 do nothing.
   /// - 您可以在此時先建立 Session Note.
   virtual void OnSessionCtor(RcClientSession& ses, const f9rc_ClientSessionParams* params);
};
//--------------------------------------------------------------------------//
class RcClientMgr : public IoManager, public RcFunctionMgrRefCounter {
   fon9_NON_COPY_NON_MOVE(RcClientMgr);
   using base = IoManager;
   using baseRef = RcFunctionMgrRefCounter;

   struct RcClientSessionFactory : public SessionFactory {
      fon9_NON_COPY_NON_MOVE(RcClientSessionFactory);
      RcClientSessionFactory();
      io::SessionSP CreateSession(IoManager& mgr, const IoConfigItem& iocfg, std::string& errReason) override;
      io::SessionServerSP CreateSessionServer(IoManager&, const IoConfigItem&, std::string& errReason) override;
   };
   RcClientSessionFactory  RcClientSessionFactory_;

   friend unsigned int intrusive_ptr_add_ref(const RcClientMgr* p) BOOST_NOEXCEPT {
      return intrusive_ptr_add_ref(static_cast<const baseRef*>(p));
   }
   friend unsigned int intrusive_ptr_release(const RcClientMgr* p) BOOST_NOEXCEPT {
      return intrusive_ptr_release(static_cast<const baseRef*>(p));
   }
   unsigned IoManagerAddRef() override;
   unsigned IoManagerRelease() override;
   void NotifyChanged(IoManager::DeviceRun&) override;
   void NotifyChanged(IoManager::DeviceItem&) override;

public:
   struct RcClientConfigItem : public IoConfigItem {
      const f9rc_ClientSessionParams*  Params_;
   };

   RcClientMgr(const IoManagerArgs& ioMgrArgs);

   int CreateRcClientSession(f9rc_ClientSession** result, const RcClientConfigItem& cfg);
   static void DestroyRcClientSession(RcClientSession* ses, int isWait);

   void OnDevice_Destructing(io::Device&) override;
   void OnDevice_Accepted(io::DeviceServer&, io::DeviceAcceptedClient&) override;
   void OnDevice_Initialized(io::Device&) override;

   void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;
   void OnDevice_StateUpdated(io::Device& dev, const io::StateUpdatedArgs& e) override;
   void OnSession_StateUpdated(io::Device& dev, StrView stmsg, LogLevel lv) override;
};

// at RcClientApi.cpp
// 在 fon9_Initialize() 建立.
extern fon9_API intrusive_ptr<RcClientMgr> RcClientMgr_;

} } // namespaces
#endif//__fon9_rc_RcClientSession_hpp__
