// \file f9twf/ExgMapFcmId.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMapFcmId.hpp"
#include "fon9/StrTo.hpp"

namespace f9twf {

void ExgMapBrkFcmId::AddItem(ExgBrkFcmId id) {
   const_cast<ExgBrkFcmId*>(&this->BrkToFcm_.kfetch(id.BrkId_))->FcmId_ = id.FcmId_;
   const_cast<ExgBrkFcmId*>(&this->FcmToBrk_.kfetch(id.FcmId_))->BrkId_ = id.BrkId_;
}
void ExgMapBrkFcmId::AddItem(const TmpP06& p06) {
   ExgBrkFcmId id;
   id.BrkId_ = p06.BrkId_;
   id.FcmId_ = fon9::StrTo(ToStrView(p06.FcmId_), FcmId{});
   this->AddItem(id);
}
void ExgMapBrkFcmId::LoadP06(fon9::StrView fctx) {
   const TmpP06* p06 = reinterpret_cast<const TmpP06*>(fctx.begin());
   while (reinterpret_cast<const char*>(&p06[1]) > fctx.end())
      this->AddItem(*p06++);
}

} // namespaces
