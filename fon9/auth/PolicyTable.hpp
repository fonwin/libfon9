/// \file fon9/PolicyTable.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicyTable_hpp__
#define __fon9_auth_PolicyTable_hpp__
#include "fon9/auth/AuthBase.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/InnDbf.hpp"
#include <map>

namespace fon9 { namespace auth {

class fon9_API PolicyTable;
class fon9_API PolicyItem;
using PolicyItemSP = intrusive_ptr<PolicyItem>;

struct CmpPolicyItemSP {
   bool operator()(const PolicyItemSP& lhs, const PolicyItemSP& rhs) const;
   bool operator()(const PolicyItemSP& lhs, const StrView& rhs) const;
   bool operator()(const StrView& lhs, const PolicyItemSP& rhs) const;
   bool operator()(const PolicyId& lhs, const PolicyItemSP& rhs) const;
   bool operator()(const PolicyItemSP& lhs, const PolicyId& rhs) const;
};
struct PolicyItemMap : public SortedVectorSet<PolicyItemSP, CmpPolicyItemSP> {
   using base = SortedVectorSet<PolicyItemSP, CmpPolicyItemSP>;
   /// 移除前呼叫: i->second->BeforeParentErase(table);
   iterator erase(iterator i);
};
using PolicyDeletedMap = std::map<PolicyId, InnDbfRoomKey>;

struct fon9_API PolicyMapsImpl {
   fon9_NON_COPY_NON_MOVE(PolicyMapsImpl);
   PolicyItemMap     ItemMap_;
   PolicyDeletedMap  DeletedMap_;

   void WriteUpdated(PolicyItem& rec);
};
using PolicyMaps = MustLock<PolicyMapsImpl>;

//--------------------------------------------------------------------------//

using PolicyItemChangedEvent = std::function<void(const PolicyItem&)>;

/// \ingroup auth
/// 一筆使用者政策設定的基底.
/// 例: Trader交易台股的權限.
class fon9_API PolicyItem : public intrusive_ref_counter<PolicyItem> {
   fon9_NON_COPY_NON_MOVE(PolicyItem);
   friend class PolicyTable;
   uint16_t       ChangedCount_{};
   bool           IsRemoved_{false};
   char           Padding___[1];
   InnDbfRoomKey  RoomKey_;

   /// 預設直接轉呼叫 this->LoadPolicy();
   virtual void LoadPolicyFromSyn(DcQueue&);
   virtual void LoadPolicyFromDbf(DcQueue&);

   virtual void LoadPolicy(DcQueue&) = 0;
   virtual void SavePolicy(RevBuffer&) = 0;

   friend struct PolicyItemMap;
   friend struct PolicyMapsImpl;
   /// 在 BeforeParentErase(), OnParentTreeClear() 時呼叫,
   /// 通常在 SetRemoved() 時切斷與 DetailTable 的聯繫.
   /// 預設: do nothing.
   virtual void SetRemoved(PolicyTable& owner);
   void BeforeParentErase(PolicyTable& owner);
   void OnAfterChanged();

public:
   const PolicyId PolicyId_;

   mutable Subject<PolicyItemChangedEvent>   AfterChangedEvent_;

   PolicyItem(const StrView& policyId)
      : PolicyId_{policyId} {
   }
   virtual ~PolicyItem();

   /// 若傳回 true, 則表示 this 已從 table 移除.
   bool IsRemoved() const {
      return this->IsRemoved_;
   }
   /// 資料異動次數.
   uint16_t ChangedCount() const {
      return this->ChangedCount_;
   }

   virtual seed::TreeSP GetSapling();

   /// 預設呼叫 BeforeParentErase(owner);
   virtual void OnParentTreeClear(PolicyTable& owner);

   /// 預設: OpResult::not_supported_cmd;
   virtual void OnSeedCommand(PolicyMaps::Locker& locker, seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler);
   virtual StrView GetSeedCommandLogStr(StrView cmdln);
};

struct PolicyItemMonitor {
   intrusive_ptr<const PolicyItem>  PolicyItem_;
   uint16_t PolicyChangedCount_;
   char     Padding____[6];
   bool IsPolicyChanged() const {
      return this->PolicyItem_ && this->PolicyItem_->ChangedCount() != this->PolicyChangedCount_;
   }
   void ResetPolicyItem(const fon9::auth::PolicyItem& item) {
      this->PolicyChangedCount_ = item.ChangedCount();
      this->PolicyItem_.reset(&item);
   }
};

//--------------------------------------------------------------------------//

/// \ingroup auth
/// 使用者政策資料表, 負責與 InnDbf 溝通.
class fon9_API PolicyTable : public InnDbfTableHandler {
   fon9_NON_COPY_NON_MOVE(PolicyTable);
public:
   PolicyTable() = default;

   bool Delete(StrView policyId);

   static PolicyTable& StaticCast(PolicyItemMap& itemMap) {
      return ContainerOf(PolicyMaps::StaticCast(ContainerOf(itemMap, &PolicyMapsImpl::ItemMap_)),
                         &PolicyTable::PolicyMaps_);
   }
protected:
   virtual PolicyItemSP MakePolicy(const StrView& policyId) = 0;
   PolicyMaps  PolicyMaps_;

private:
   struct HandlerBase {
      PolicyId Key_;
      static InnDbfRoomKey& GetRoomKey(PolicyDeletedMap::value_type& v) {
         return v.second;
      }
      static InnDbfRoomKey& GetRoomKey(PolicyItemMap::value_type& v) {
         return v->RoomKey_;
      }
      PolicyItem& FetchPolicy(PolicyTable& table, PolicyItemMap& itemMap, PolicyItemMap::iterator* iItem) {
         if (iItem)
            return ***iItem;
         PolicyItemSP rec = table.MakePolicy(ToStrView(this->Key_));
         return **itemMap.insert(std::move(rec)).first;
      }
   };

   struct LoadHandler : public HandlerBase, public InnDbfLoadHandler {
      fon9_NON_COPY_NON_MOVE(LoadHandler);
      using InnDbfLoadHandler::InnDbfLoadHandler;
      void UpdateLoad(PolicyItemMap& itemMap, PolicyItemMap::iterator* iItem);
      void UpdateLoad(PolicyDeletedMap& deletedMap, PolicyDeletedMap::iterator* iDeleted);
   };
   virtual void OnInnDbfTable_Load(InnDbfLoadEventArgs& e) override;

   struct SyncHandler : public HandlerBase, public InnDbfSyncHandler {
      fon9_NON_COPY_NON_MOVE(SyncHandler);
      SyncHandler(InnDbfTableHandler& handler, InnDbfSyncEventArgs& e)
         : InnDbfSyncHandler(handler, e) {
      }

      InnDbfRoomKey* PendingWriteRoomKey_{nullptr};
      RevBufferList  PendingWriteBuf_{128};

      void UpdateSync(PolicyItemMap& itemMap, PolicyItemMap::iterator* iItem);
      void UpdateSync(PolicyDeletedMap& deletedMap, PolicyDeletedMap::iterator* iDeleted);
   };
   virtual void OnInnDbfTable_Sync(InnDbfSyncEventArgs& e) override;
   virtual void OnInnDbfTable_SyncFlushed() override;
};

inline PolicyItemMap::iterator PolicyItemMap::erase(iterator i) {
   (*i)->BeforeParentErase(PolicyTable::StaticCast(*this));
   return base::erase(i);
}

//struct CmpPolicyItemSP {
   inline bool CmpPolicyItemSP::operator()(const PolicyItemSP& lhs, const PolicyItemSP& rhs) const {
      return lhs->PolicyId_ < rhs->PolicyId_;
   }
   inline bool CmpPolicyItemSP::operator()(const PolicyItemSP& lhs, const StrView& rhs) const {
      return ToStrView(lhs->PolicyId_) < rhs;
   }
   inline bool CmpPolicyItemSP::operator()(const StrView& lhs, const PolicyItemSP& rhs) const {
      return lhs < ToStrView(rhs->PolicyId_);
   }
   inline bool CmpPolicyItemSP::operator()(const PolicyId& lhs, const PolicyItemSP& rhs) const {
      return lhs < rhs->PolicyId_;
   }
   inline bool CmpPolicyItemSP::operator()(const PolicyItemSP& lhs, const PolicyId& rhs) const {
      return lhs->PolicyId_ < rhs;
   }
//};

} } // namespaces
#endif//__fon9_auth_PolicyTable_hpp__
