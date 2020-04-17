// \file fon9/fmkt/SymbQuoteReq.hpp
//
// 商品詢價要求 相關欄位.
//
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbQuoteReq_hpp__
#define __fon9_fmkt_SymbQuoteReq_hpp__
#include "fon9/fmkt/Symb.hpp"
#include "fon9/TimeInterval.hpp"

namespace fon9 { namespace fmkt {

struct SymbQuoteReq_Data {
   /// 詢價揭示時間.
   DayTime  DisclosureTime_{DayTime::Null()};
   /// 詢價持續時間.
   uint16_t DurationSeconds_{};
   char     Padding___[6];

   void Clear() {
      this->DisclosureTime_.AssignNull();
      this->DurationSeconds_ = 0;
   }
   bool operator==(const SymbQuoteReq_Data& rhs) const {
      return this->DisclosureTime_ == rhs.DisclosureTime_
         && this->DurationSeconds_ == rhs.DurationSeconds_;
   }
   bool operator!=(const SymbQuoteReq_Data& rhs) const {
      return !this->operator==(rhs);
   }
};

/// \ingroup fmkt
/// 商品資料的擴充: 詢價要求.
fon9_API_TEMPLATE_CLASS(SymbQuoteReq, SimpleSymbData, SymbQuoteReq_Data);

fon9_API seed::Fields SymbQuoteReq_MakeFields();

} } // namespaces
#endif//__fon9_fmkt_SymbQuoteReq_hpp__
