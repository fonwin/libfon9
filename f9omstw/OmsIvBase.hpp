// \file f9omstw/OmsIvBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsIvBase_hpp__
#define __f9omstw_OmsIvBase_hpp__
#include "fon9/sys/Config.hpp"
#include "fon9/intrusive_ref_counter.hpp"

namespace f9omstw {

class OmsIvBase;
using OmsIvBaseSP = fon9::intrusive_ptr<OmsIvBase>;

/// OmsIvBase 衍生出:「子帳」、「投資人帳號」、「券商」.
class OmsIvBase : public fon9::intrusive_ref_counter<OmsIvBase> {
   fon9_NON_COPY_NON_MOVE(OmsIvBase);
public:
   char  padding___[4];

   /// - OmsSubac 的 Parent = OmsIvac;
   /// - OmsIvac 的 Parent = OmsBrk;
   /// - OmsBrk 的 Parent = nullptr;
   const OmsIvBaseSP Parent_;

   OmsIvBase(OmsIvBaseSP parent) : Parent_{std::move(parent)} {
   }
   virtual ~OmsIvBase();
};

class OmsBrkTree;
class OmsBrk;
using OmsBrkSP = fon9::intrusive_ptr<OmsBrk>;

class OmsIvacTree;
class OmsIvac;
using OmsIvacSP = fon9::intrusive_ptr<OmsIvac>;

class OmsSubac;
using OmsSubacSP = fon9::intrusive_ptr<OmsSubac>;

class OmsCore;
class OmsTree;

} // namespaces
#endif//__f9omstw_OmsIvBase_hpp__
