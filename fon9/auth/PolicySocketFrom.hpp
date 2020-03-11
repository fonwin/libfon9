/// \file fon9/auth/PolicySocketFrom.hpp
///
/// 檢查 SocketAddress 來源, 是否在黑名單 or 白名單.
///
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicySocketFrom_hpp__
#define __fon9_auth_PolicySocketFrom_hpp__
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/io/Socket.hpp"
#include "fon9/ConfigUtils.hpp"
#include "fon9/seed/FieldString.hpp"

namespace fon9 { namespace auth {

class fon9_API SocketAddressRangeStr : public EvStringT<CharVector> {
   io::SocketAddressRange  Range_;
   void OnStringAfterChanged() override;
public:
   bool InRange(const io::SocketAddress& addr) const {
      return this->Range_.InRange(addr);
   }
};

struct fon9_API PolicySocketAddressRangeRec : public PolicyItem {
   fon9_NON_COPY_NON_MOVE(PolicySocketAddressRangeRec);
   using PolicyItem::PolicyItem;
   void LoadPolicy(DcQueue&) override;
   void SavePolicy(RevBuffer&) override;

   SocketAddressRangeStr   AddressRange_;
   EnabledYN               IsAllow_{};
   char                    Padding____[7];

   static seed::Fields MakeFields();
};

/// - PolicyId = 檢查順序.
///   - 預期不會有太多黑白名單, 所以使用依序方式尋找.
///   - 配合 PolicyTree、PolicyItem 的儲存方式.
///   - 所以使用 PolicyId 當 key, 而不用 SocketAddress.
///   - PolicyId 當 key 也可以視為註解, e.g. "i8610" 表示 8610 分公司的設定.
class fon9_API PolicySocketFromAgent : public PolicyAgent {
   fon9_NON_COPY_NON_MOVE(PolicySocketFromAgent);
   using base = PolicyAgent;
   class Tree;

public:
   PolicySocketFromAgent(const seed::MaTree* authMgrAgents, std::string name);

   static intrusive_ptr<PolicySocketFromAgent> Plant(AuthMgr& authMgr, std::string name) {
      auto res = authMgr.Agents_->Plant<PolicySocketFromAgent>("PolicySocketFromAgent.Plant", std::move(name));
      if (res)
         res->LinkStorage(authMgr.Storage_, sizeof(PolicySocketAddressRangeRec));
      return res;
   }

   /// 如果沒有設定, 預設為 false;
   bool OnBeforeAccept(const io::SocketAddress& addrRemote, io::SocketResult& soRes, CharVector& exlog);
};
using PolicySocketFromAgentSP = intrusive_ptr<PolicySocketFromAgent>;

} } // namespaces
#endif//__fon9_auth_PolicySocketFrom_hpp__
