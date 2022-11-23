/// \file fon9/fmkt/TradingLineGroup.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_TradingLineGroup_hpp__
#define __fon9_fmkt_TradingLineGroup_hpp__
#include "fon9/fmkt/TradingRequest.hpp"
#include "fon9/seed/MaTree.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 每一個 LineGroup 可能包含多個 LineMgr, 例:
/// 證券的 LineGroup 包含: 上市線路群組、上櫃線路群組;
/// 期權的 LineGroup 包含: 期貨日盤、期貨夜盤、選擇權日盤、選擇權夜盤;
class fon9_API TradingLgMgrBase : public seed::NamedMaTree {
   fon9_NON_COPY_NON_MOVE(TradingLgMgrBase);
   using base = seed::NamedMaTree;
protected:
   // class LgItem : public LgItemBase, public LineGroup;
   using LgItemBase = seed::NamedMaTree;
   using LgItemBaseSP = intrusive_ptr<LgItemBase>;
   LgItemBaseSP LgItems_[static_cast<uint8_t>(LgOut::Count)];

   virtual LgItemBaseSP CreateLgItem(Named&& named) = 0;
   virtual TradingLineManager* GetLineMgr(LgItemBase& lg, const TradingRequest& req) const = 0;
   /// 參考 ref(例:上市線路管理員), 從 lg 取出相同的 LineMgr.
   virtual TradingLineManager* GetLineMgr(LgItemBase& lg, const TradingLineManager& ref) const = 0;
   virtual TradingLineManager* GetLineMgr(LgItemBase& lg, unsigned lmgrIndex) const = 0;

#define fon9_kCSTR_LgTAG_LEAD    "Lg"
#define fon9_kSIZE_LgTAG_LEAD    (sizeof(fon9_kCSTR_LgTAG_LEAD) - 1)
   /// tag:   "Lg?"; 例: "Lg0"
   /// cfgln: "Title, Description"; 例: "VIP線路";
   LgItemBaseSP MakeLgItem(StrView tag, StrView cfgln);

public:
   using base::base;
   ~TradingLgMgrBase();

   LgItemBase* GetLgItem(unsigned lgIndex) const {
      return lgIndex < numofele(this->LgItems_) ? this->LgItems_[lgIndex].get() : nullptr;
   }
   /// 根據 lg 選用適當的線路群組, 若 lg 沒有設定線路群組, 則使用 lg='0' 通用線路.
   LgItemBase* GetLgItem(LgOut lg) const {
      if (LgItemBase* retval = this->LgItems_[LgOutToIndex(lg)].get())
         return retval;
      return this->LgItems_[0].get();
   }
   /// LgIndex = LgOut or unsigned;
   /// LgRef = TradingRequest or TradingLineManager or unsigned(同一組裡面的index,例:[0]=上市,[1]=上櫃);
   template <typename LgIndex, typename LgRef>
   TradingLineManager* GetLineMgr(LgIndex lgIndex, const LgRef& ref) const {
      if (auto* lg = this->GetLgItem(lgIndex))
         return this->GetLineMgr(*lg, ref);
      return nullptr;
   }
   /// 每一個 Lg 裡面有幾個 LineMgr. 例:
   /// - 台股=2:   上市,上櫃.
   /// - 台期權=4: 期貨日盤,期貨夜盤,選擇權日盤,選擇權夜盤.
   virtual unsigned LgLineMgrCount() const = 0;
};

} } // namespaces
#endif//__fon9_fmkt_TradingLineGroup_hpp__
