// \file fon9/fmkt/FmktTools.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/FmktTools.hpp"

namespace fon9 { namespace fmkt {

fon9_API void CalcLmt(const LvPriStep* steps, Pri ref, double rate, Pri* pUpLmt, Pri* pDnLmt) {
   const Pri   adj{ref.To<double>() * rate};
   if (pDnLmt) {
      fon9::fmkt::Pri dn = ref - adj;
      if (fon9_UNLIKELY(!pDnLmt->IsNull() && dn <= *pDnLmt))
         *pDnLmt = steps->Step_;
      else {
         for (;;) {
            if (dn <= steps->LvTop_) {
               const auto mod = dn % steps->Step_;
               if (mod.GetOrigValue() != 0)
                  dn += (steps->Step_ - mod);
               break;
            }
            ++steps;
         }
         *pDnLmt = dn;
      }
   }
   if (pUpLmt) {
      fon9::fmkt::Pri up = ref + adj;
      for (;;) {
         if (up <= steps->LvTop_) {
            up -= (up % steps->Step_);
            break;
         }
         ++steps;
      }
      if (pUpLmt->IsNull() || up < *pUpLmt)
         *pUpLmt = up;
      else
         *pUpLmt -= steps->Step_;
   }
}

} } // namespaces
