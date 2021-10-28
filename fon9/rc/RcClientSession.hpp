// \file fon9/rc/RcClientSession.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcClientSession_hpp__
#define __fon9_rc_RcClientSession_hpp__
#include "fon9/rc/RcSession.hpp"
#include "fon9/rc/RcClientApi.h"
#include "fon9/framework/IoManager.hpp"

struct fon9_IoManager : public fon9::IoManager, public fon9::rc::RcFunctionMgrRefCounter {
private:
   fon9_NON_COPY_NON_MOVE(fon9_IoManager);
   using baseIoMgr = IoManager;
   using baseRef = fon9::rc::RcFunctionMgrRefCounter;

   friend unsigned int intrusive_ptr_add_ref(const fon9_IoManager* p) BOOST_NOEXCEPT {
      return intrusive_ptr_add_ref(static_cast<const baseRef*>(p));
   }
   friend unsigned int intrusive_ptr_release(const fon9_IoManager* p) BOOST_NOEXCEPT {
      return intrusive_ptr_release(static_cast<const baseRef*>(p));
   }
   unsigned IoManagerAddRef() override;
   unsigned IoManagerRelease() override;
   void NotifyChanged(baseIoMgr::DeviceRun&) override;
   void NotifyChanged(baseIoMgr::DeviceItem&) override;
   baseIoMgr::DeviceItemSP CreateSession(const fon9_IoSessionParams& params);
   bool AddSessionAndOpen(baseIoMgr::DeviceItemSP item);

public:
   /// 預設加入 2 個 Device factory:
   ///   this->DeviceFactoryPark_->Add(MakeIoFactoryTcpClient("TcpClient"));
   ///   this->DeviceFactoryPark_->Add(fon9::MakeIoFactoryDgram("Dgram"));
   fon9_IoManager(const fon9::IoManagerArgs& ioMgrArgs);

   static void DestroyDevice(fon9::io::DeviceSP dev, std::string cause, int isWait);

   /// \retval nullptr  失敗 *pses = nullptr;
   /// \retval !nullptr 建立&開啟成功, *pses 為新建立出來的 session;
   ///           在開啟成功後, 返回前, 就有可能觸發相關事件.
   ///           所以若有需要處理 session 的事件, 則建議使用 pses 取得返回值.
   template <class Session>
   Session* OpenSession(Session** pses, const fon9_IoSessionParams& params) {
      if (auto item = this->CreateSession(params)) {
         if (auto ses = dynamic_cast<Session*>(item->Device_->Session_.get())) {
            if (pses)
               *pses = ses;
            if (this->AddSessionAndOpen(item))
               return ses;
         }
      }
      if (pses)
         *pses = nullptr;
      return nullptr;
   }
};

//--------------------------------------------------------------------------//

namespace fon9 { namespace rc {

class fon9_API RcClientSession;

class fon9_API RcClientSessionEvHandler {
   fon9_NON_COPY_NON_MOVE(RcClientSessionEvHandler);
public:
   RcClientSessionEvHandler() = default;
   virtual ~RcClientSessionEvHandler();

   /// - 在 RcClientSession::OnDevice_StateChanged(); 相關事項處理前觸發事件.
   /// - 所以在 RcClientSession、RcFunctionNote 清理資源前, 可以先收到事件.
   ///   避免在清理資源後收到事件, 會有可能用到被清理掉的資源.
   virtual void OnRcClientSession_BeforeLinkEv(RcClientSession& rcSes) = 0;
   /// - 在 RcClientSession::ApReady(); 觸發事件.
   virtual void OnRcClientSession_ApReady(RcClientSession& rcSes) = 0;
};

class fon9_API RcClientSession : public RcSession, public f9rc_ClientSession {
   fon9_NON_COPY_NON_MOVE(RcClientSession);
   using base = RcSession;
   using EvHandlers = fon9::Subject<RcClientSessionEvHandler*>;
   EvHandlers  EvHandlers_;
   std::string Password_;

protected:
   void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;
   void SetApReady(StrView info) override;

public:
   const f9rc_FnOnLinkEv   FnOnLinkEv_;

   RcClientSession(RcFunctionMgrSP funcMgr, const f9rc_ClientSessionParams* params);

   StrView GetAuthPassword() const override;

   void AddEvHandler(RcClientSessionEvHandler& h) {
      this->EvHandlers_.Subscribe(&h);
   }
   void DelEvHandler(RcClientSessionEvHandler& h) {
      this->EvHandlers_.Unsubscribe(&h); 
   }
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
class RcClientMgr : public fon9_IoManager {
   fon9_NON_COPY_NON_MOVE(RcClientMgr);
   using base = fon9_IoManager;

   struct RcClientSessionFactory : public SessionFactory {
      fon9_NON_COPY_NON_MOVE(RcClientSessionFactory);
      RcClientSessionFactory();
      io::SessionSP CreateSession(IoManager& mgr, const IoConfigItem& iocfg, std::string& errReason) override;
      io::SessionServerSP CreateSessionServer(IoManager&, const IoConfigItem&, std::string& errReason) override;
   };
   RcClientSessionFactory  RcClientSessionFactory_;

public:
   struct RcClientConfigItem : public IoConfigItem {
      const f9rc_ClientSessionParams*  Params_;
   };

   RcClientMgr(const IoManagerArgs& ioMgrArgs);

   int CreateRcClientSession(f9rc_ClientSession** result, const RcClientConfigItem& cfg);
   static void DestroyRcClientSession(RcClientSession* ses, int isWait);

   void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;
   void OnDevice_StateUpdated(io::Device& dev, const io::StateUpdatedArgs& e) override;
   void OnSession_StateUpdated(io::Device& dev, StrView stmsg, LogLevel lv) override;
};

// at RcClientApi.cpp
// 在 fon9_Initialize() 建立.
extern fon9_API intrusive_ptr<RcClientMgr> RcClientMgr_;

} } // namespaces
#endif//__fon9_rc_RcClientSession_hpp__
