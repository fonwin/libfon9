// \file fon9/fmkt/SymbTimePri.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTimePri_hpp__
#define __fon9_fmkt_SymbTimePri_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

struct SymbTimePri_Data {
   /// 此價格的時間.
   DayTime  Time_{DayTime::Null()};
   /// 價格(Open or High or Low...).
   Pri      Pri_{Pri::Null()};

   void Clear() {
      this->Time_.AssignNull();
      this->Pri_.AssignNull();
   }
};

/// \ingroup fmkt
/// 商品資料擴充: 時間 + 單一價格.
/// 一般用在 Open/High/Low 之類的欄位.
fon9_API_TEMPLATE_CLASS(SymbTimePri, SimpleSymbData, SymbTimePri_Data);

fon9_API seed::Fields SymbTimePri_MakeFields(std::string priName);
//--------------------------------------------------------------------------//
// inline void CheckSymbTimePriOpen(SymbTimePri& dst, Pri pri, DayTime tm) {
//    if (dst.Data_.Time_.IsNull()) {
//       dst.Data_.Time_ = tm;
//       dst.Data_.Pri_ = pri;
//    }
// }
// inline void CheckSymbTimePriHigh(SymbTimePri& dst, Pri pri, DayTime tm) {
//    if (dst.Data_.Pri_ < pri || dst.Data_.Time_.IsNull()) {
//       dst.Data_.Time_ = tm;
//       dst.Data_.Pri_ = pri;
//    }
// }
// inline void CheckSymbTimePriLow(SymbTimePri& dst, Pri pri, DayTime tm) {
//    if (dst.Data_.Pri_ > pri || dst.Data_.Time_.IsNull()) {
//       dst.Data_.Time_ = tm;
//       dst.Data_.Pri_ = pri;
//    }
// }
//--------------------------------------------------------------------------//
class SymbDataOHL {
   fon9_NON_COPY_NON_MOVE(SymbDataOHL);
public:
   SymbDataOHL() = default;

   SymbTimePri Open_;
   SymbTimePri High_;
   SymbTimePri Low_;

   void CheckOHL(Pri pri, DayTime tm) {
      if (fon9_LIKELY(!this->Open_.Data_.Time_.IsNull())) {
         if (fon9_UNLIKELY(this->High_.Data_.Pri_ < pri)) {
            this->High_.Data_.Time_ = tm;
            this->High_.Data_.Pri_ = pri;
         }
         if (fon9_UNLIKELY(this->Low_.Data_.Pri_ > pri)) {
            this->Low_.Data_.Time_ = tm;
            this->Low_.Data_.Pri_ = pri;
         }
      }
      else {
         this->Open_.Data_.Pri_ = pri;
         this->Open_.Data_.Time_ = tm;
         this->High_.Data_ = this->Low_.Data_ = this->Open_.Data_;
      }
   }

   #define f9fmkt_MAKE_TABS_OpenHighLow() \
   fon9::seed::TabSP{new fon9::seed::Tab{fon9::Named{fon9_kCSTR_TabName_Open}, fon9::fmkt::SymbTimePri_MakeFields("Open"), fon9::seed::TabFlag::NoSapling_NoSeedCommand_Writable}}, \
   fon9::seed::TabSP{new fon9::seed::Tab{fon9::Named{fon9_kCSTR_TabName_High}, fon9::fmkt::SymbTimePri_MakeFields("High"), fon9::seed::TabFlag::NoSapling_NoSeedCommand_Writable}}, \
   fon9::seed::TabSP{new fon9::seed::Tab{fon9::Named{fon9_kCSTR_TabName_Low},  fon9::fmkt::SymbTimePri_MakeFields("Low"),  fon9::seed::TabFlag::NoSapling_NoSeedCommand_Writable}}

   #define f9fmkt_MAKE_OFFSET_OpenHighLow(SymbT) \
   fon9_OffsetOf(SymbT, Open_), \
   fon9_OffsetOf(SymbT, High_), \
   fon9_OffsetOf(SymbT, Low_)
};

} } // namespaces
#endif//__fon9_fmkt_SymbTimePri_hpp__
