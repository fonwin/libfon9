// \file f9twf/TwfSymbRef.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_TwfSymbRef_hpp__
#define __f9twf_TwfSymbRef_hpp__
#include "f9twf/ExgTypes.hpp"
#include "fon9/fmkt/Symb.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9twf {

struct TwfSymbRef_Data {
   fon9::fmkt::Pri   PriRef_{};

   /// 昨日收盤價.
   fon9::fmkt::Pri   PPriClose_{};
   /// 昨日總量.
   fon9::fmkt::Qty   PQtyTotal_{};
   /// 未平倉量.
   fon9::fmkt::Qty   OpenInterest_{};

   /// 期交所漲跌停價, 在文件「逐筆行情資訊傳輸作業手冊」版本 Ver:1.2.0 (2020/08/03) 改成使用 I012 提供.
   /// 漲跌停階數為 PackBcd<2>; 所以最多為 99 階. 原則上最多不超過 20;
   enum {
      kPriLmtCount = 20
   };

   struct PriLmt {
      fon9::fmkt::Pri   Up_{};
      fon9::fmkt::Pri   Dn_{};
      PriLmt() {
      }
      PriLmt(fon9::fmkt::Pri up, fon9::fmkt::Pri dn) : Up_{up}, Dn_{dn} {
      }
      void Reset(fon9::fmkt::Pri up, fon9::fmkt::Pri dn) {
         this->Up_ = up;
         this->Dn_ = dn;
      }
   };
   PriLmt   PriLmts_[kPriLmtCount];
   char     PaddingAfterPriLmts___[6];

   /// define: union { LvLmts_[2]; struct { LvUpLmt_, LvDnLmt_ }};
   f9twf_DEF_MEMBERS_LvLmts;

   TwfSymbRef_Data() {
      this->Clear();
   }
   void Clear() {
      fon9::ForceZeroNonTrivial(this);
   }
   void ClearPriLmts() {
      memset(static_cast<void*>(this->PriLmts_), 0, sizeof(this->PriLmts_));
   }
   bool IsEuqalPriRefAndPriLmts(const TwfSymbRef_Data& rhs) const {
      return memcmp(this, &rhs, fon9_OffsetOf(TwfSymbRef_Data, PaddingAfterPriLmts___)) == 0;
   }
};

// gcc 不喜歡把這行寫在 namespace f9twf 裡面, 所以就移到 namespace 外面, 避免 gcc 的警告.
// template class fon9::fmkt::SimpleSymbData<f9twf::TwfSymbRef_Data> f9twf_API;
using TwfSymbRef = fon9::fmkt::SimpleSymbData<TwfSymbRef_Data>;

f9twf_API fon9::seed::Fields TwfSymbRef_MakeFields();
f9twf_API void TwfSymbRef_AddPriLmtFields(fon9::seed::Fields& flds, unsigned ifrom);

//--------------------------------------------------------------------------//

/// 漲停: leg1=upLmt, leg2=upLmt; 跌停: leg1=dnLmt, leg2=dnLmt;
static inline fon9::fmkt::Pri CombPriLmt_SameSide(fon9::fmkt::Pri leg1, fon9::fmkt::Pri leg2) {
   return (leg1.IsNullOrZero() || leg2.IsNullOrZero()) ? fon9::fmkt::Pri::Null() : (leg1 + leg2);
}
/// 漲停: leg1=upLmt, leg2=dnLmt; 跌停: leg1=dnLmt, leg2=upLmt;
static inline fon9::fmkt::Pri CombPriLmt_SideIsLeg1(fon9::fmkt::Pri leg1, fon9::fmkt::Pri leg2) {
   return (leg1.IsNullOrZero() || leg2.IsNullOrZero()) ? fon9::fmkt::Pri::Null() : (leg1 - leg2);
}
/// 價差單價格: (Leg1=近月, Leg2=遠月);
///    漲停價 = (遠月漲停價－近月跌停價);
///    跌停價 = (遠月跌停價－近月漲停價);
/// 漲停: leg1=dnLmt, leg2=upLmt; 跌停: leg1=upLmt, leg2=dnLmt;
static inline fon9::fmkt::Pri CombPriLmt_SideIsLeg2(fon9::fmkt::Pri leg1, fon9::fmkt::Pri leg2) {
   return (leg1.IsNullOrZero() || leg2.IsNullOrZero()) ? fon9::fmkt::Pri::Null() : (leg2 - leg1);
}

static inline fon9::fmkt::Pri TwfGetLmtPri(const TwfSymbRef_Data& symbRefData, TwfLvLmts contractLvLmts, UDIdx udIdx) {
   assert(&symbRefData.LvUpLmt_ + 1 == &symbRefData.LvDnLmt_);
   assert(&symbRefData.PriLmts_->Up_ + 1 == &symbRefData.PriLmts_->Dn_);
   assert(udIdx == UDIdx_Up || udIdx == UDIdx_Dn);
   const auto lvLmt = TwfGetLmtLv(contractLvLmts[udIdx], symbRefData.LvLmts_[udIdx]);
   return (lvLmt < symbRefData.kPriLmtCount ? (&symbRefData.PriLmts_[lvLmt].Up_)[udIdx] : fon9::fmkt::Pri::Null());
}

/// 計算複式商品的漲跌停價.
f9twf_API bool TwfGetCombLmtPri(ExgCombSide combSide, const TwfSymbRef_Data& leg1, const TwfSymbRef_Data& leg2,
                                TwfLvLmts contractLvLmts, fon9::fmkt::Pri* upLmt, fon9::fmkt::Pri* dnLmt);

//--------------------------------------------------------------------------//
} // namespaces
template class fon9::fmkt::SimpleSymbData<f9twf::TwfSymbRef_Data> f9twf_API;

#endif//__f9twf_TwfSymbRef_hpp__
