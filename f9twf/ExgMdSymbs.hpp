// \file f9twf/ExgMdSymbs.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdSymbs_hpp__
#define __f9twf_ExgMdSymbs_hpp__
#include "f9twf/Config.h"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/fmkt/SymbDeal.hpp"

namespace f9twf {

class f9twf_API ExgMdSymb : public fon9::fmkt::Symb {
   fon9_NON_COPY_NON_MOVE(ExgMdSymb);
   using base = fon9::fmkt::Symb;
public:
   /// 用來判斷資料的新舊.
   fon9::DayTime        BasicInfoTime_{fon9::DayTime::Null()};
   fon9::fmkt::SymbRef  Ref_;
   fon9::fmkt::SymbBS   BS_;
   fon9::fmkt::SymbDeal Deal_;

   using base::base;

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

   void DailyClear();

   static fon9::seed::LayoutSP MakeLayout();
};
//--------------------------------------------------------------------------//
class f9twf_API ExgMdSymbs : public fon9::fmkt::SymbTree {
   fon9_NON_COPY_NON_MOVE(ExgMdSymbs);
   using base = fon9::fmkt::SymbTree;
public:
   ExgMdSymbs();

   void DailyClear();

   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;
};
using ExgMdSymbsSP = fon9::intrusive_ptr<ExgMdSymbs>;
//--------------------------------------------------------------------------//
struct ExgMdEntry;

/// 返回 mdEntry + mdCount;
f9twf_API const void* ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry,
                                         fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv);
template <class SymbT>
inline const void* ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry, SymbT& symb) {
   return ExgMdEntryToSymbBS(mdTime, mdCount, mdEntry, symb.BS_, symb.PriceOrigDiv_);
}
//----------------------
struct ExgMcI081Entry;

f9twf_API void ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry,
                                  fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv);
template <class SymbT>
inline void ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry, SymbT& symb) {
   ExgMdEntryToSymbBS(mdTime, mdCount, mdEntry, symb.BS_, symb.PriceOrigDiv_);
}

} // namespaces
#endif//__f9twf_ExgMdSymbs_hpp__
