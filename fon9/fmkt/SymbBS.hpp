// \file fon9/fmkt/SymbBS.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBS_hpp__
#define __fon9_fmkt_SymbBS_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

enum class BSFlag : uint8_t {
   /// 試撮後剩餘委託簿.
   Calculated = 0x01,
   /// 委託簿的買方有異動.
   OrderBuy = 0x02,
   /// 委託簿的賣方有異動.
   OrderSell = 0x04,
   /// 委託簿的衍生買方有異動.
   DerivedBuy = 0x10,
   /// 委託簿的衍生賣方有異動.
   DerivedSell = 0x20,
};
fon9_ENABLE_ENUM_BITWISE_OP(BSFlag);

/// \ingroup fmkt
/// 商品資料的擴充: 行情的買賣報價.
class fon9_API SymbBS : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbBS);
public:
   enum {
      /// 買賣價量列表數量.
      kBSCount = 5,
   };
   struct Data {
      /// 報價時間.
      DayTime  InfoTime_{DayTime::Null()};
      /// 賣出價量列表, [0]=最佳賣出價量.
      PriQty   Sells_[kBSCount];
      /// 買進價量列表, [0]=最佳買進價量.
      PriQty   Buys_[kBSCount];
      /// 衍生賣出.
      PriQty   DerivedSell_;
      /// 衍生買進.
      PriQty   DerivedBuy_;
      BSFlag   Flags_{};
      char     Padding___[7];
   };
   Data  Data_;
   
   SymbBS(const Data& rhs) : Data_{rhs} {
   }
   SymbBS() = default;

   void Clear(DayTime tm = DayTime::Null()) {
      memset(&this->Data_, 0, sizeof(this->Data_));
      this->Data_.InfoTime_ = tm;
   }
   void DailyClear() {
      this->Clear();
   }

   static seed::Fields MakeFields();
};

class fon9_API SymbBSTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbBSTabDy);
   using base = SymbDataTab;
public:
   SymbBSTabDy(Named&& named)
      : base{std::move(named), SymbBS::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbBS_hpp__
