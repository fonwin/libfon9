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

fon9_API Pri MoveTicksUp(const LvPriStep* steps, Pri priFrom, uint16_t count, const Pri upLmt) {
   if (priFrom >= upLmt)
      return upLmt;
   if (steps == nullptr)
      return priFrom;
   if (steps->LvTop_ == Pri::max()) {
      const Pri mod = (priFrom % steps->Step_);
      if (fon9_UNLIKELY(!mod.IsZero())) {
         priFrom -= mod;
         if (fon9_UNLIKELY(mod.GetOrigValue() < 0))
            priFrom -= steps->Step_;
      }
      return std::min(priFrom + steps->Step_ * count, upLmt);
   }

   using count_t = decltype(count);
   if (fon9_LIKELY(priFrom.GetOrigValue() >= 0)) {
      steps = FindPriTickStep(steps, priFrom);
      if (priFrom == steps->LvTop_)
         ++steps;
      else {
         priFrom -= (priFrom % steps->Step_);
      }
      if (count <= 0) {
         return std::min(priFrom, upLmt);
      }
   }
   else {
      const LvPriStep* const begin = steps;
      // priFrom 從 <0 走到 =0, 若 count 仍有剩餘, 則從最小 tick(begin) 開始繼續往上走.
      // => 將 priFrom 改成正數, 並往下走, 最多走到 0; 然後再往上走.
      priFrom = Pri{} - priFrom;
      steps = FindPriTickStep(steps, priFrom);
      const Pri mod = (priFrom % steps->Step_);
      if (fon9_UNLIKELY(!mod.IsZero()))
         priFrom += (steps->Step_ - mod);
      if (count <= 0) {
         return std::min(Pri{} - priFrom, upLmt);
      }
      for (;;) {
         const Pri  nextTop = (steps == begin ? Pri{} : (steps - 1)->LvTop_);
         Pri        nextPri = priFrom - steps->Step_ * count;
         const bool isFound = (nextTop <= nextPri);
         nextPri = Pri{} - nextPri;
         if (upLmt <= nextPri)
            return upLmt;
         if(isFound)
            return nextPri;
         const auto lvCount = (priFrom - nextTop).GetOrigValue() / steps->Step_.GetOrigValue();
         assert(count > lvCount);
         count = static_cast<count_t>(count - static_cast<count_t>(lvCount));
         priFrom = nextTop;
         if (steps == begin)
            break;
         --steps;
      }
      assert(count > 0 && priFrom.IsZero());
   }

   assert(count > 0 && priFrom < steps->LvTop_);
   for (;;) {
      const Pri nextPri = priFrom + steps->Step_ * count;
      if (upLmt <= nextPri)
         return upLmt;
      if (nextPri <= steps->LvTop_)
         return nextPri;
      const auto lvCount = (steps->LvTop_ - priFrom).GetOrigValue() / steps->Step_.GetOrigValue();
      assert(count > lvCount);
      count = static_cast<count_t>(count - static_cast<count_t>(lvCount));
      priFrom = steps->LvTop_;
      ++steps;
   }
}
fon9_API Pri MoveTicksDn(const LvPriStep* steps, Pri priFrom, uint16_t count, Pri dnLmt) {
   if (priFrom <= dnLmt)
      return dnLmt;
   if (steps == nullptr)
      return priFrom;
   if (steps->LvTop_ == Pri::max()) {
      const Pri mod = (priFrom % steps->Step_);
      if (fon9_UNLIKELY(!mod.IsZero())) {
         priFrom -= mod;
         if (fon9_LIKELY(mod.GetOrigValue() > 0))
            priFrom += steps->Step_;
      }
      return std::max(priFrom - steps->Step_ * count, dnLmt);
   }

   using count_t = decltype(count);
   if (fon9_LIKELY(priFrom.GetOrigValue() > 0)) {
      const LvPriStep* const begin = steps;
      // priFrom>0 往下走, 若走到 =0, 且 count 仍有剩餘, 則從最小 tick(begin) 繼續往下走.
      steps = FindPriTickStep(steps, priFrom);
      const Pri mod = (priFrom % steps->Step_);
      if (fon9_UNLIKELY(!mod.IsZero()))
         priFrom += (steps->Step_ - mod);
      if (count <= 0) {
         return std::max(priFrom, dnLmt);
      }
      for (;;) {
         const Pri nextBtm = (steps == begin ? Pri{} : (steps - 1)->LvTop_);
         const Pri nextPri = priFrom - steps->Step_ * count;
         if (nextPri <= dnLmt)
            return dnLmt;
         if (nextBtm <= nextPri)
            return nextPri;
         const auto lvCount = (priFrom - nextBtm).GetOrigValue() / steps->Step_.GetOrigValue();
         assert(count > lvCount);
         count = static_cast<count_t>(count - static_cast<count_t>(lvCount));
         priFrom = nextBtm;
         if (steps == begin)
            break;
         --steps;
      }
      // 繼續往下走, 就變成負值了...
      assert(count > 0 && priFrom.IsZero());
   }
   else {
      // priFrom <=0, 往下走, 找到適當的起始 step 之後, 一直往下走即可.
      priFrom = Pri{} - priFrom;
      steps = FindPriTickStep(steps, priFrom);
      if (priFrom == steps->LvTop_)
         ++steps;
      else {
         priFrom -= (priFrom % steps->Step_);
      }
      if (count <= 0) {
         return std::max(Pri{} - priFrom, dnLmt);
      }
   }

   assert(count > 0 && priFrom < steps->LvTop_);
   for (;;) {
      Pri        nextPri = priFrom + steps->Step_ * count;
      const bool isFound = (nextPri <= steps->LvTop_);
      nextPri = Pri{} - nextPri;
      if (nextPri <= dnLmt)
         return dnLmt;
      if (isFound)
         return nextPri;
      const auto lvCount = (steps->LvTop_ - priFrom).GetOrigValue() / steps->Step_.GetOrigValue();
      assert(count > lvCount);
      count = static_cast<count_t>(count - static_cast<count_t>(lvCount));
      priFrom = steps->LvTop_;
      ++steps;
   }
}

} } // namespaces
