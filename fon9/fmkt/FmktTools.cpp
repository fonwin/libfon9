// \file fon9/fmkt/FmktTools.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/FmktTools.hpp"
#include <stdlib.h>

namespace fon9 { namespace fmkt {

void LvPriStepVect::FreeThis() {
   free(this->Vect_);
   this->Vect_ = nullptr;
}
void LvPriStepVect::FromStr(fon9::StrView cfgs) {
   this->FreeThis();
   unsigned   cap = 0, idxNext = 0;
   LvPriStep* res = nullptr;
   LvPriStep  lv;
   fon9::StrView tag, value;
   while (fon9::StrFetchTagValue(cfgs, tag, value)) {
      if (value.IsNull()) { // 沒有找到 chEqual: 最後一個 step;
         lv.LvTop_ = Pri::max();
         if ((lv.Step_ = fon9::StrTo(tag, Pri{})).IsNullOrZero())
            continue;
      }
      else {
         if ((lv.LvTop_ = fon9::StrTo(tag, Pri{})).IsNullOrZero())
            continue;
         if ((lv.Step_ = fon9::StrTo(value, Pri{})).IsNullOrZero())
            continue;
      }
      if (idxNext >= cap) {
         cap += 20;
         res = reinterpret_cast<LvPriStep*>(realloc(res, sizeof(LvPriStep) * cap));
      }
      res[idxNext++] = lv;
      if (lv.LvTop_ == Pri::max()) {
         this->Vect_ = res;
         return;
      }
   }
   if (res) {
      res[idxNext - 1].LvTop_ = Pri::max();
      this->Vect_ = res;
   }
}

fon9_API CharVector LvPriStep_ToStr(const LvPriStep* steps) {
   CharVector  retval;
   char        outbuf[256];
   char* const pend = outbuf + sizeof(outbuf);
   while (steps) {
      char* pout = ToStrRev(pend, steps->Step_);
      if (steps->LvTop_ != Pri::max()) {
         *--pout = '=';
         pout = ToStrRev(pout, steps->LvTop_);
      }
      if (!retval.empty())
         *--pout = '|';
      retval.append(pout, pend);
      if (steps->LvTop_ == Pri::max())
         break;
      ++steps;
   }
   return retval;
}

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
