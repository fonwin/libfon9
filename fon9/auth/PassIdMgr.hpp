// \file fon9/auth/PassIdMgr.hpp
//
// PassIdMgr: 協助管理密碼設定
//    在系統的設定裡面使用 PassKey="PassId/Salt"; 可以避免在設定中出現[明碼].
//
// \author fonwinz@gmail.com
#ifndef __fon9_auth_PassIdMgr_hpp__
#define __fon9_auth_PassIdMgr_hpp__
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 { namespace auth {

struct fon9_API PassIdRec : public PolicyItem {
   fon9_NON_COPY_NON_MOVE(PassIdRec);
   using base = PolicyItem;

protected:
   void LoadPolicy(DcQueue&) override;
   void SavePolicy(RevBuffer&) override;

   /// 支援使用 repw pass
   /// 傳回 "PassId/Salt"
   void OnSeedCommand(PolicyMaps::Locker& locker, seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler) override;
   StrView GetSeedCommandLogStr(StrView cmdln) override;

public:
   using base::base;

   ByteVector  SaltedPass_;
   TimeStamp   ChangedTime_ = TimeStamp::Null();
   /// 不儲存, 僅顯示最後的 PassKey.
   CharVector  PassKey_;

   static seed::Fields MakeFields();
};

//--------------------------------------------------------------------------//

class fon9_API PassIdTree : public PolicyTree {
   fon9_NON_COPY_NON_MOVE(PassIdTree);
   using base = PolicyTree;
protected:
   PolicyItemSP MakePolicy(const StrView& passId) override {
      return PolicyItemSP{new PassIdRec{passId}};
   }

public:
   using Locker = PolicyMaps::Locker;

   PassIdTree();

   bool GetPass(StrView passKey, CharVector& password) const;
};

class fon9_API PassIdMgr : public PolicyAgent {
   fon9_NON_COPY_NON_MOVE(PassIdMgr);
   using base = PolicyAgent;
public:
   PassIdMgr(const seed::MaTree* authMgrAgents, std::string name);

   static intrusive_ptr<PassIdMgr> Plant(AuthMgr& authMgr, std::string name) {
      auto res = authMgr.Agents_->Plant<PassIdMgr>("PassIdMgr.Plant", std::move(name));
      if (res)
         res->LinkStorage(authMgr.Storage_, sizeof(PassIdRec));
      return res;
   }
   /// 若成功, 則設定 fon9::SetFnPassKeyToPassword();
   static intrusive_ptr<PassIdMgr> PlantPassKeyMgr(AuthMgr& authMgr, std::string name);

   /// passKey = "PassId/Salt"
   bool GetPass(const StrView& passKey, CharVector& password) const {
      return static_cast<PassIdTree*>(this->Sapling_.get())->GetPass(passKey, password);
   }

   enum {
      kPassKeyMaxLength = 15
   };
};
using PassIdMgrSP = intrusive_ptr<PassIdMgr>;

} } // namespaces
#endif//__fon9_auth_PassIdMgr_hpp__
