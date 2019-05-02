// \file tws/TwsSubac.hpp
//
// 台股子帳的資料結構.
//
// \author fonwinz@gmail.com
#ifndef __f9omstw_tws_TwsSubac_hpp__
#define __f9omstw_tws_TwsSubac_hpp__
#include "f9omstw/OmsSubac.hpp"
#include "tws/TwsIvSc.hpp"

namespace f9omstw {

/// 子帳基本資料.
struct TwsSubacInfo {
   fon9::fmkt::BigAmt   OrderLimit_;
};

class TwsSubac : public OmsSubac {
   fon9_NON_COPY_NON_MOVE(TwsSubac);
   using base = OmsSubac;
   struct PodOp;
public:
   TwsSubacInfo   Info_;
   TwsIvSc        Sc_;

   using base::base;

   static fon9::seed::LayoutSP MakeLayout();
   /// 建立 grid view, 包含 SubacNo_; 不含尾端分隔符號.
   void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) override;
   void OnPodOp(OmsTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) override;
};
using TwsSubacSP = fon9::intrusive_ptr<TwsSubac>;

} // namespaces
#endif//__f9omstw_tws_TwsSubac_hpp__
