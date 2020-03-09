/// \file fon9/BitvFixedInt.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_BitvFixedInt_hpp__
#define __fon9_BitvFixedInt_hpp__
#include "fon9/BitvEncode.hpp"
#include "fon9/IntSel.hpp"

namespace fon9 {

/// \ingroup AlNum
/// ToBitv(rbuf, BitvFixedUInt<n> v); 固定使用 n bytes 記錄(加上fon9_BitvT, 共 n + 1 bytes);
/// - 通常用在整筆料使用Bitv格式儲存, 但某欄位特別需要使用固定長度, 例如:
///   - InnApf.FileExHeader: key使用Bitv, FirstRoomPos 使用 BitvFixedUInt<>;
/// - 解碼時也可以直接使用 BitvTo(rbuf, uintXX_t& v);
template <size_t kBitvFixedLen>
struct BitvFixedUInt {
   enum { kBitvLen = kBitvFixedLen };
   using Type = UIntTypeSelector_t<kBitvLen>;
   Type Value_;
};

template <size_t kBitvLen, typename T>
inline char* ToBitvFixedRev(char* pout, T val) {
   for (size_t L = 0; ;) {
      *--pout = static_cast<char>(val);
      if (++L >= kBitvLen)
         break;
      fon9_GCC_WARN_DISABLE("-Wconversion");
      val >>= 8;
      fon9_GCC_WARN_POP;                                                                   
   }
   fon9_MSC_WARN_DISABLE(4333);// '>>' : right shift by too large amount, data loss
   assert((val >> 8) == 0);
   fon9_MSC_WARN_POP;
   return pout;
}

template <size_t kBitvLen>
inline size_t ToBitv(RevBuffer& rbuf, BitvFixedUInt<kBitvLen> src) {
   char* pout = rbuf.AllocPrefix(kBitvLen + 1);
   rbuf.SetPrefixUsed(pout - (kBitvLen + 1));
   pout = ToBitvFixedRev<kBitvLen>(pout, src.Value_);
   *(pout - 1) = static_cast<char>(fon9_BitvT_IntegerPos | (kBitvLen - 1));
   return kBitvLen + 1;
}

template <size_t kBitvLen>
inline void BitvTo(DcQueue& buf, BitvFixedUInt<kBitvLen>& dst) {
   return BitvTo(buf, dst.Value_);
}
//--------------------------------------------------------------------------//
/// \ingroup AlNum
/// 通常用於幾乎填滿小數位的情況, 例如: DayTime;
/// - 減少為了移除「小數位尾端的0」的除法.
/// - 支援 fon9_BitvV_Number0, fon9_BitvV_NumberNull;
/// - 非 0, 非 Null, 則小數固定 = ScaleN, 輸出動態長度.
template <typename IntTypeT, DecScaleT kScaleN>
inline size_t ToBitvFixedScale(RevBuffer& rbuf, Decimal<IntTypeT, kScaleN> src) {
   char* pout;
   if (src.IsZero())
      *(pout = rbuf.AllocPrefix(1) - 1) = fon9_BitvV_Number0;
   else if (src.IsNull())
      *(pout = rbuf.AllocPrefix(1) - 1) = fon9_BitvV_NumberNull;
   else { // 動態總長度, 固定小數位.
      return DecToBitv(rbuf, src.GetOrigValue(), kScaleN, src.GetOrigValue() < 0);
   }
   rbuf.SetPrefixUsed(pout);
   return 1;
}
/// \ingroup AlNum
/// - 2 <= kBitvBitSize && kBitvBitSize <= sizeof(uintmax_t) * 8
///   - 小數位固定 = ScaleN. 輸出總長度包含fon9_BitvT = (2 + (kBitvBitSize + 7 - 2) / 8).
///   - 不支援 Null;
/// - kBitvBitSize 可配合 CalcValueBits() 使用, 例如:
///   - DayTime 一日間所需的 bits = CalcValueBits((24 * 60 * 60 + 1) * DayTime::Divisor);
///   - TimeStamp 所需的 bits = CalcValueBits(0x100000000 * TimeStamp::Divisor);
template <size_t kBitvBitSize, typename IntTypeT, DecScaleT kScaleN>
inline size_t ToBitvFixedScale(RevBuffer& rbuf, Decimal<IntTypeT, kScaleN> src) {
   static_assert(2 <= kBitvBitSize && kBitvBitSize <= sizeof(uintmax_t) * 8, "不正確的 kBitvBitSize");
   const size_t kBitvLen = 1 + ((kBitvBitSize + 7 - 2) / 8); // +1 = 包含 2 bits 的額外空間.
   char* pout = rbuf.AllocPrefix(kBitvLen + 1);
   rbuf.SetPrefixUsed(pout - (kBitvLen + 1));
   pout = ToBitvFixedRev<kBitvLen>(pout, abs_cast(src.GetOrigValue()));
   assert((*pout & ~0x03) == 0);
   *pout = static_cast<char>(*pout | (kScaleN << 3));
   if (src.GetOrigValue() < 0)
      *pout = static_cast<char>(*pout | 0x40);
   *(pout - 1) = static_cast<char>(fon9_BitvT_Decimal | (kBitvLen - 2));
   return kBitvLen + 1;
}

template <typename T>
constexpr unsigned CalcValueBits(T val) {
   return val ? (CalcValueBits(unsigned_cast(val) >> 1) + 1) : 0;
}
static_assert(CalcValueBits(0x3ff) == 10, "CalcValueBits() err.");

} // namespace fon9
#endif//__fon9_BitvFixedInt_hpp__
