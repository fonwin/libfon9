// \file f9tws/TwsTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_TwsTools_hpp__
#define __f9tws_TwsTools_hpp__
#include "f9tws/TwsTypes.h"
#include "f9tws/ExgTypes.hpp"
#include "fon9/fmkt/FmktTools.hpp"

inline f9tws_SymbKind StrTo(fon9::StrView s, f9tws_SymbKind null, const char** endptr = nullptr) {
   return static_cast<f9tws_SymbKind>(fon9::HexStrTo(s, endptr, null));
}
inline char* ToStrRev(char* pout, f9tws_SymbKind v) {
   return fon9::HexToStrRev(pout, fon9::cast_to_underlying(v));
}
//--------------------------------------------------------------------------//
namespace f9tws {

struct f9tws_API TwsSymbKindLvPriStep {
   f9tws_SymbKind                Kind_;
   char                          Padding____[7];
   const fon9::fmkt::LvPriStep*  LvPriSteps_;
   
   TwsSymbKindLvPriStep() = default;
   TwsSymbKindLvPriStep(StkNo stkno) {
      this->Setup(stkno);
   }
   void Setup(StkNo stkno);
};

} // namespaces
#endif//__f9tws_TwsTools_hpp__
