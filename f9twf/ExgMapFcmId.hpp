// \file f9twf/ExgMapFcmId.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMapFcmId_hpp__
#define __f9twf_ExgMapFcmId_hpp__
#include "f9twf/ExgTypes.hpp"
#include "fon9/SortedVector.hpp"

namespace f9twf {

fon9_WARN_DISABLE_PADDING;
struct TmpP06 {
   BrkId             BrkId_;
   fon9::CharAryF<5> FcmId_;
};
static_assert(sizeof(TmpP06) == 7 + 5, "struct TmpP06 must pack?");

/// 7碼期貨商代號 - FcmId(數字序號) 對照.
struct ExgBrkFcmId {
   BrkId BrkId_;
   FcmId FcmId_;
   ExgBrkFcmId() = default;
   ExgBrkFcmId(const BrkId& brkid) : BrkId_{brkid} {}
   ExgBrkFcmId(FcmId fcmid) : FcmId_{fcmid} {}
};
struct ExgBrkFcmId_CmpBrkId {
   bool operator()(const ExgBrkFcmId& lhs, const ExgBrkFcmId& rhs) const {
      return lhs.BrkId_ < rhs.BrkId_;
   }
   bool operator()(const BrkId& lhs, const ExgBrkFcmId& rhs) const {
      return lhs < rhs.BrkId_;
   }
   bool operator()(const ExgBrkFcmId& lhs, const BrkId& rhs) const {
      return lhs.BrkId_ < rhs;
   }
};
struct ExgBrkFcmId_CmpFcmId {
   bool operator()(const ExgBrkFcmId& lhs, const ExgBrkFcmId& rhs) const {
      return lhs.FcmId_ < rhs.FcmId_;
   }
   bool operator()(FcmId lhs, const ExgBrkFcmId& rhs) const {
      return lhs < rhs.FcmId_;
   }
   bool operator()(const ExgBrkFcmId& lhs, FcmId rhs) const {
      return lhs.FcmId_ < rhs;
   }
};

/// 7碼期貨商代號 - FcmId(數字序號) 對照表.
class f9twf_API ExgMapBrkFcmId {
   using BrkToFcm = fon9::SortedVectorSet<ExgBrkFcmId, ExgBrkFcmId_CmpBrkId>;
   using FcmToBrk = fon9::SortedVectorSet<ExgBrkFcmId, ExgBrkFcmId_CmpFcmId>;
   BrkToFcm BrkToFcm_;
   FcmToBrk FcmToBrk_;

public:
   void reserve(size_t sz) {
      this->BrkToFcm_.reserve(sz);
      this->FcmToBrk_.reserve(sz);
   }
   void swap(ExgMapBrkFcmId& rhs) {
      this->BrkToFcm_.swap(rhs.BrkToFcm_);
      this->FcmToBrk_.swap(rhs.FcmToBrk_);
   }

   void AddItem(ExgBrkFcmId id);
   void AddItem(const TmpP06& p06);

   /// 載入期交所P06, 格式: (BrkId[7]FcmId[5]) * N 筆, 沒換行.
   /// BrkId 一律使用大寫.
   void LoadP06(fon9::StrView fctx);

   BrkId GetBrkId(FcmId id) const {
      auto ifind = this->FcmToBrk_.find(id);
      return ifind == this->FcmToBrk_.end() ? BrkId{nullptr} : ifind->BrkId_;
   }
   FcmId GetFcmId(BrkId id) const {
      auto ifind = this->BrkToFcm_.find(id);
      return ifind == this->BrkToFcm_.end() ? FcmId{0} : ifind->FcmId_;
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9twf_ExgMapFcmId_hpp__
