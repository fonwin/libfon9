// \file f9omstw/OmsIvac.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvac_hpp__
#define __f9omstw_OmsIvac_hpp__
#include "f9omstw/OmsIvBase.hpp"
#include "f9omstw/IvacNo.hpp"
#include "fon9/LevelArray.hpp"
#include "fon9/seed/PodOp.hpp"

namespace f9omstw {

class OmsIvac : public OmsIvBase {
   fon9_NON_COPY_NON_MOVE(OmsIvac);
   using base = OmsIvBase;
public:
   /// 帳號包含檢查碼.
   const IvacNo   IvacNo_;
   char           padding___[4];

   OmsIvac(IvacNo ivacNo, OmsBrkSP parent);
   ~OmsIvac();

   /// 建立 grid view, 包含 IvacNo_; 不含尾端分隔符號.
   virtual void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) = 0;
   virtual void OnPodOp(OmsIvacTree& ownerTree, fon9::seed::FnPodOp&& fnCallback, const fon9::StrView& strKeyText) = 0;
   virtual void OnParentSeedClear() = 0;
};

/// key = 移除檢查碼之後的帳號;
using OmsIvacMap = fon9::LevelArray<IvacNC, OmsIvacSP, 3>;

} // namespaces
#endif//__f9omstw_OmsIvac_hpp__
