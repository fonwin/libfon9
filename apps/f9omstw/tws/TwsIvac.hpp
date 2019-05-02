// \file tws/TwsIvac.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_tws_TwsIvac_hpp__
#define __f9omstw_tws_TwsIvac_hpp__
#include "tws/TwsIvSc.hpp"
#include "f9omstw/OmsIvac.hpp"
#include "f9omstw/OmsSubac.hpp"
#include "fon9/LevelArray.hpp"

namespace f9omstw {

class TwsSubac;
using TwsSubacSP = fon9::intrusive_ptr<TwsSubac>;

/// 投資帳號基本資料.
struct TwsIvacInfo {
   fon9::CharVector     Name_;
   /// 投資上限.
   fon9::fmkt::BigAmt   OrderLimit_;
};

class TwsIvac : public OmsIvac {
   fon9_NON_COPY_NON_MOVE(TwsIvac);
   using base = OmsIvac;
   struct PodOp;

   OmsSubacMap Subacs_;

public:
   TwsIvacInfo Info_;
   TwsIvSc     Sc_;

   using base::base;

   // 除非確實無此子帳, 否則不應刪除 subac, 因為:
   // - 若已有正確委託, 則該委託風控異動時, 仍會使用移除前的 TwsSubacSP.
   // - 若之後又建立一筆相同 SubacNo 的資料, 則會與先前移除的 TwsSubacSP 不同!
   TwsSubacSP RemoveSubac(fon9::StrView subacNo);
   TwsSubac* FetchSubac(fon9::StrView subacNo);
   TwsSubac* GetSubac(fon9::StrView subacNo) const;

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag treeflags);
   /// 建立 grid view, 包含 IvacNo_; 不含尾端分隔符號.
   void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) override;
   void OnPodOp(OmsIvacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) override;
   void OnParentSeedClear() override;
};
using TwsIvacSP = fon9::intrusive_ptr<TwsIvac>;

} // namespaces
#endif//__f9omstw_tws_TwsIvac_hpp__
