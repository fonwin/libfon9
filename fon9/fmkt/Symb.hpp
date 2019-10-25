// \file fon9/fmkt/Symb.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_Symb_hpp__
#define __fon9_fmkt_Symb_hpp__
#include "fon9/fmkt/FmktTypes.h"
#include "fon9/CharVector.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/Trie.hpp"
#include "fon9/SortedVector.hpp"
#include "fon9/seed/Tab.hpp"

#include <unordered_map>

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 為了讓 SymbTree::PodOp 裡面的 seed::SimpleRawRd{*symbdata} 可以正確運作,
/// SymbData 的衍生者, 必須將 「SymbData 基底」放在第一位,
/// 請參考 class Symb; class SymbRef;
class fon9_API SymbData {
   fon9_NON_COPY_NON_MOVE(SymbData);
public:
   SymbData() = default;
   virtual ~SymbData();
};

using SymbFlowGroup_t = uint8_t;
using SymbSeqNo_t = uint16_t;

/// \ingroup fmkt
/// 可擴充的商品資料.
/// - 配合 seed::LayoutDy 或 LayoutN 機制擴充, 例如:
///   - 參考價.
///   - 買賣報價(量).
///   - 成交價量.
class fon9_API Symb : public SymbData, public intrusive_ref_counter<Symb> {
   fon9_NON_COPY_NON_MOVE(Symb);
public:
   /// 台灣證券「一張」的單位數(股數)
   uint32_t ShUnit_{0};

   /// 交易市場.
   f9fmkt_TradingMarket    TradingMarket_{f9fmkt_TradingMarket_Unknown};
   /// 交易時段.
   /// - 台灣證券: 不提供.
   /// - 台灣期權: 在載入 P08.{SystemType} 時, 根據 SystemType 填入此值.
   f9fmkt_TradingSessionId TradingSessionId_{f9fmkt_TradingSessionId_Unknown};

   char  Filler_____[1];

   /// 台灣期交所的流程群組代碼.
   SymbFlowGroup_t   FlowGroup_{0};
   /// 交易所的價格欄位, 若沒有定義小數位, 則使用 PriceOrigDiv_ 來處理;
   /// - 例如: 期交所Tmp格式的下單、回報, 都需要用 PriceOrigDiv_ 來決定小數位:
   ///   - 若 TaiFexPri = Decimal<int64_t,9>;
   ///   - 期交所 P08 的 decimal_locator = 2;
   ///   - 則 PriceOrigDiv_ = 10^7;
   uint32_t PriceOrigDiv_{0};
   /// 選擇權商品代碼, 履約價格部份之小數調整.
   /// - 例如: "TEO03200L9" 電子 320.0 買權;
   ///   此時商品Id的履約價:小數1位; 則 StrikePriceDiv_ = 10;
   uint32_t StrikePriceDiv_{0};

   /// 期交所定義的商品序號.
   SymbSeqNo_t ExgSymbSeq_{0};
   char        Filler2_____[2];

   /// 系統自訂的商品Id, 不一定等於交易所的商品Id.
   /// 交易送出、行情解析時, 再依自訂規則轉換.
   const CharVector  SymbId_;

   Symb(const StrView& symbid) : SymbId_{symbid} {
   }

   virtual SymbData* GetSymbData(int tabid);
   virtual SymbData* FetchSymbData(int tabid);

   static seed::Fields MakeFields();
};
using SymbSP = intrusive_ptr<Symb>;

/// 取得台灣證券的整股交易單位, 若沒設定, 則 1張 = 1000股.
inline uint32_t GetTwsSymbShUnit(const Symb* symb) {
   if (symb && 0 < symb->ShUnit_)
      return symb->ShUnit_;
   return 1000;
}

//--------------------------------------------------------------------------//

using SymbHashMap = std::unordered_map<StrView, SymbSP>;
inline Symb& GetSymbValue(const std::pair<StrView, SymbSP>& v) {
   return *v.second;
}
inline StrView GetSymbKey(const std::pair<StrView, SymbSP>& v) {
   return v.first;
}

template <class Symbs>
inline SymbSP InsertSymb(Symbs& symbs, SymbSP v) {
   return symbs.insert(typename Symbs::value_type(ToStrView(v->SymbId_), v)).first->second;
}

//--------------------------------------------------------------------------//

struct SymbSP_Comparer {
   bool operator()(const SymbSP& lhs, const SymbSP& rhs) const {
      return lhs->SymbId_ < rhs->SymbId_;
   }
   bool operator()(const StrView& lhs, const SymbSP& rhs) const {
      return lhs < ToStrView(rhs->SymbId_);
   }
   bool operator()(const SymbSP& lhs, const StrView& rhs) const {
      return ToStrView(lhs->SymbId_) < rhs;
   }
};
using SymbSortedVector = SortedVectorSet<SymbSP, SymbSP_Comparer>;
inline Symb& GetSymbValue(const SymbSP& v) {
   return *v;
}
inline StrView GetSymbKey(const SymbSP& v) {
   return ToStrView(v->SymbId_);
}
inline SymbSP InsertSymb(SymbSortedVector& symbs, SymbSP&& v) {
   return *symbs.insert(std::move(v)).first;
}

//--------------------------------------------------------------------------//

/// 僅允許 ASCII 裡面的: 0x20(space)..0x5f(_) 當作 key, 不支援小寫及「 ` { | } ~ 」
/// 如果有超過範圍的字元, 則視為相同, 放在該層的最後. 例如: "A12|4" == "A12~4".
/// 無效字元 ToKey() 一律傳回 '~'
struct TrieSymbKey {
   using LvKeyT = char;
   using LvIndexT = fon9::byte;
   using key_type = fon9::StrView;
   static constexpr LvIndexT MaxIndex() {
      return 0x5f - 0x20 + 1;
   }
   fon9_GCC_WARN_DISABLE("-Wconversion"); // 抑制 val -= 0x20; idx += 0x20; 造成的 warning.
   static LvIndexT ToIndex(LvKeyT val) {
      return static_cast<LvIndexT>(val -= 0x20) > MaxIndex()
         ? MaxIndex() : static_cast<LvIndexT>(val);
   }
   static LvKeyT ToKey(LvIndexT idx) {
      return (idx += 0x20) > 0x5f ? '~' : static_cast<LvKeyT>(idx);
   }
   fon9_GCC_WARN_POP;
};
using SymbTrieMap = Trie<TrieSymbKey, SymbSP>;
inline Symb& GetSymbValue(const SymbTrieMap::value_type& v) {
   return *v.value();
}
inline std::string GetSymbKey(const SymbTrieMap::value_type& v) {
   return v.key();
}
inline SymbSP InsertSymb(SymbTrieMap& symbs, SymbSP v) {
   return symbs.emplace(ToStrView(v->SymbId_), v).first->value();
}

//--------------------------------------------------------------------------//

/// \ingroup fmkt
/// - 根據 Symb_UT.cpp 的測試結果, std::unordered_map 比 std::map、fon9::Trie
///   更適合用來處理「全市場」的「商品資料表」, 因為資料筆數較多, 且商品Id散亂(不適合使用Trie)。
/// - 但是如果是「個別帳戶」的「商品庫存」(個別的資料筆數較少, 但帳戶可能很多),
///   則仍需其他驗證, 才能決定用哪種容器: 「帳戶庫存表」應考慮使用較少記憶體的容器。
using SymbMap = SymbHashMap;
//using SymbMap = SymbTrieMap;

} } // namespaces
#endif//__fon9_fmkt_Symb_hpp__
