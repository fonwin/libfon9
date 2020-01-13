// \file f9twf/ExgMdPkReceiver.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdPkReceiver_hpp__
#define __f9twf_ExgMdPkReceiver_hpp__
#include "f9twf/ExgMdFmt.hpp"
#include "fon9/PkReceiver.hpp"

namespace f9twf {

/// 解析台灣期交所「間隔行情」格式框架.
class f9twf_API ExgMiPkReceiver : public fon9::PkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMiPkReceiver);
   using base = fon9::PkReceiver;
public:
   ExgMiPkReceiver(bool isDgram = false)
      : base{sizeof(ExgMiHead), isDgram} {
   }
   virtual ~ExgMiPkReceiver();

   unsigned GetPkSize(const void* pkptr) override;
};

/// 解析台灣期交所「逐筆行情」格式框架.
class f9twf_API ExgMcPkReceiver : public fon9::PkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMcPkReceiver);
   using base = fon9::PkReceiver;
public:
   ExgMcPkReceiver (bool isDgram = false)
      : base{sizeof(ExgMcHead), isDgram} {
   }
   virtual ~ExgMcPkReceiver();

   unsigned GetPkSize(const void* pkptr) override;
};

} // namespaces
#endif//__f9twf_ExgMdPkReceiver_hpp__
