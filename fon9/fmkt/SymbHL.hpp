// \file fon9/fmkt/SymbHL.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbHL_hpp__
#define __fon9_fmkt_SymbHL_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料擴充: 行情當日成交最高價.
class fon9_API SymbHigh : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbHigh);
public:
   struct Data {
      /// 最高價成交時間.
      DayTime  Time_{DayTime::Null()};
      /// 最高價.
      Pri      PriHigh_{};

      void DailyClear() {
         // memset(this, 0, sizeof(*this));
         this->Time_.AssignNull();
         this->PriHigh_.AssignNull();
      }
   };
   Data  Data_;

   SymbHigh(const Data& rhs) : Data_{rhs} {
   }
   SymbHigh() = default;

   void DailyClear() {
      this->Data_.DailyClear();
   }

   static seed::Fields MakeFields();

   void CheckHigh(Pri pri, DayTime tm) {
      if (this->Data_.PriHigh_ < pri || this->Data_.Time_.IsNull()) {
         this->Data_.Time_ = tm;
         this->Data_.PriHigh_ = pri;
      }
   }
};

/// \ingroup fmkt
/// 商品資料擴充: 行情當日成交最低價.
class fon9_API SymbLow : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbLow);
public:
   struct Data {
      /// 最低價成交時間.
      DayTime  Time_{DayTime::Null()};
      /// 最低價.
      Pri      PriLow_{};

      void DailyClear() {
         // memset(this, 0, sizeof(*this));
         this->Time_.AssignNull();
         this->PriLow_.AssignNull();
      }
   };
   Data  Data_;

   SymbLow(const Data& rhs) : Data_{rhs} {
   }
   SymbLow() = default;

   void DailyClear() {
      this->Data_.DailyClear();
   }

   static seed::Fields MakeFields();

   void CheckLow(Pri pri, DayTime tm) {
      if (this->Data_.PriLow_ > pri || this->Data_.Time_.IsNull()) {
         this->Data_.Time_ = tm;
         this->Data_.PriLow_ = pri;
      }
   }
};

} } // namespaces
#endif//__fon9_fmkt_SymbHL_hpp__
