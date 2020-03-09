// \file fon9/BitvDecode.hpp
//
// Bitv Decode:
// - 在解包前, 必須已經先確定: buf 資料量足夠, 期望的資料型別.
// - 若型別錯誤: 則拋出 BitvTypeNotMatch 異常.
// - 若 buf 的資料量不足: 則拋出 BitvNeedsMore 異常.
// - 只有 PopBitvByteArraySize() 允許資料不足.
//
// \author fonwinz@gmail.com
#ifndef __fon9_BitvDecode_hpp__
#define __fon9_BitvDecode_hpp__
#include "fon9/Bitv.h"
#include "fon9/Decimal.hpp"
#include "fon9/CharAry.hpp"
#include "fon9/Endian.hpp"
#include "fon9/Exception.hpp"
#include "fon9/buffer/DcQueue.hpp"
#include <array>

extern "C" { struct fon9_Blob; } // #include "fon9/Blob.h"
namespace fon9 { struct CharVector; } // #include "fon9/CharVector.hpp"

namespace fon9 {

fon9_MSC_WARN_DISABLE(4623/*default constructor was implicitly defined as deleted*/);
fon9_DEFINE_EXCEPTION(BitvDecodeError,  std::runtime_error);
fon9_DEFINE_EXCEPTION(BitvNeedsMore,    BitvDecodeError);
fon9_DEFINE_EXCEPTION(BitvTypeNotMatch, BitvDecodeError);
fon9_DEFINE_EXCEPTION(BitvUnknownValue, BitvDecodeError);
fon9_DEFINE_EXCEPTION(BitvSignedError,  BitvDecodeError);
fon9_MSC_WARN_POP;

inline bool IsBitvIntegerNeg(const DcQueue& buf) {
   if (const byte* p1 = buf.Peek1())
      return ((*p1 & fon9_BitvT_Mask) == fon9_BitvT_IntegerNeg);
   return false;
}
inline bool IsBitvByteArray(const DcQueue& buf) {
   if (const byte* p1 = buf.Peek1())
      return ((*p1 & 0x80) == 0x00)
         || (*p1 == fon9_BitvV_ByteArrayEmpty)
         || ((*p1 & fon9_BitvT_Mask) == fon9_BitvT_ByteArray);
   return false;
}

//--------------------------------------------------------------------------//

fon9_API void BitvToChar(DcQueue& buf, char& out);
inline void BitvTo(DcQueue& buf, char& out) {
   BitvToChar(buf, out);
}

fon9_API void BitvToBool(DcQueue& buf, bool& out);
inline void BitvTo(DcQueue& buf, bool& out) {
   BitvToBool(buf, out);
}

//--------------------------------------------------------------------------//

enum class PeekBitvByteArraySizeR : int {
   /// 資料不足, 無法取得「儲存長度的部分」
   NeedsMoreHead = -1,
   /// 正確取出了長度, 長度存放在 barySize.
   /// 但 buf 的資料量 < (「儲存長度的部分」 + barySize).
   NeedsMoreData = 0,
   /// retval >= PeekBitvByteArraySizeR::DataReady
   /// - 正確取出了長度, 長度存放在 barySize.
   /// - buf 的資料量必定 >= (「儲存長度的部分」 + barySize).
   /// - retval = 「儲存長度的部分」.
   DataReady = 1,
};

/// \ingroup AlNum
/// 取得 ByteArray 資料的長度.
/// 若資料型別不是 ByteArray 則會拋出 BitvTypeNotMatch 異常.
fon9_API PeekBitvByteArraySizeR PeekBitvByteArraySize(const DcQueue& buf, size_t& barySize);
   
/// \ingroup AlNum
/// 取得 ByteArray 資料的長度.
/// 若資料型別不是 ByteArray 則會拋出 BitvTypeNotMatch 異常.
/// \retval true 正確取出了長度(且剩餘資料量足夠), 長度存放在 barySize.
///   - 此時 buf 會移除「儲存長度的部分」, 且 buf 的剩餘資料量必定 >= barySize;
///   - 通常在返回後應取出接下來的資料.
/// \retval false 資料不足, buf 內容不變.
///   - 若 barySize==0, 則表示 buf 的資料量不足「儲存長度的部分」,
///   - 若 barySize>0, 則表示 buf 的資料量不足 barySize.
fon9_API bool PopBitvByteArraySize(DcQueue& buf, size_t& barySize);

//--------------------------------------------------------------------------//
// BitvTo 動態大小的 byte array: std::string, ByteVector, CharVector...

fon9_API void BitvToStrAppend(DcQueue& buf, std::string& dst);
fon9_API void BitvToStrAppend(DcQueue& buf, CharVector& dst);
fon9_API void BitvToBlob(DcQueue& buf, fon9_Blob& dst);
inline void BitvTo(DcQueue& buf, fon9_Blob& dst) {
   BitvToBlob(buf, dst);
}

template <class StrT>
inline void BitvToStrAppend(DcQueue& buf, StrT& dst) {
   size_t   byteCount;
   if (!PopBitvByteArraySize(buf, byteCount))
      Raise<BitvNeedsMore>("BitvToStrAppend: needs more");
   if (byteCount > 0) {
      dst.reserve(dst.size() + byteCount);
      for (;;) {
         auto   blk{buf.PeekCurrBlock()};
         assert(blk.second > 0);
         if (blk.second > byteCount)
            blk.second = static_cast<decltype(blk.second)>(byteCount);
         dst.append(reinterpret_cast<const char*>(blk.first), blk.second);
         buf.PopConsumed(blk.second);
         if ((byteCount -= blk.second) <= 0)
            break;
      }
   }
}

template <class StrT>
inline void BitvToStr(DcQueue& buf, StrT& dst) {
   dst.clear();
   BitvToStrAppend(buf, dst);
}

template <class StrT>
inline auto BitvTo(DcQueue& buf, StrT& dst)
-> decltype(dst.clear(), dst.reserve(1u), dst.append(static_cast<const char*>(nullptr), 1u), void()) {
   dst.clear();
   BitvToStrAppend(buf, dst);
}

//--------------------------------------------------------------------------//
// BitvTo 固定大小的 byte array: char[], byte[], CharAry<N,char>...

/// 從 buf 取出 bitv 格式的 byte array.
/// - 若 bitv 儲存的資料量超過 barySize: 則會丟出 std::out_of_range
/// - 若 bitv 儲存的資料量 < barySize: 則在 bary 尾端填滿 0.
/// - 傳回實際存入 bary 的資料量, 必定 <= barySize.
fon9_API size_t BitvToByteArray(DcQueue& buf, void* bary, size_t barySize);

template <typename T, size_t arysz>
inline enable_if_t<IsTrivialByte<T>::value, size_t>
BitvTo(DcQueue& buf, T (&ary)[arysz]) {
   return BitvToByteArray(buf, ary, arysz);
}

template <typename T, size_t arysz>
inline enable_if_t<IsTrivialByte<T>::value, size_t>
BitvTo(DcQueue& buf, std::array<T, arysz>& ary) {
   return BitvToByteArray(buf, ary.data(), arysz);
}

template <typename T, size_t arysz>
inline enable_if_t<IsTrivialByte<T>::value, size_t>
BitvTo(DcQueue& buf, CharAry<arysz, T>& ary) {
   return BitvToByteArray(buf, ary.Chars_, arysz);
}

//--------------------------------------------------------------------------//

template <class T,
   fon9_BitvT BitvT_Pos = fon9_BitvT_IntegerPos,
   fon9_BitvV BitvV_0 = fon9_BitvV_Number0,
   fon9_BitvV BitvV_Null = fon9_BitvV_NumberNull
>
inline T BitvToIntAux(DcQueue& buf, T nullValue) {
   if (const byte* const ptype = buf.Peek1()) {
      const fon9_BitvT type = static_cast<fon9_BitvT>(*ptype & fon9_BitvT_Mask);
      if (fon9_LIKELY(type == BitvT_Pos)) {
__POP_INT_VALUE:
         byte byteCount = static_cast<byte>((*ptype & 0x07) + 1);
         if (fon9_LIKELY(byteCount <= sizeof(T))) {
            uintmax_t numbuf[2];
            if (const byte* pnum = static_cast<const byte*>(buf.Peek(numbuf, byteCount + 1u))) {
               nullValue = GetPackedBigEndian<T>(pnum + 1, byteCount);
               buf.PopConsumed(byteCount + 1u);
               return type == fon9_BitvT_IntegerNeg ? static_cast<T>(~nullValue) : nullValue;
            }
            Raise<BitvNeedsMore>("BitvToInt: needs more");
         }
         Raise<std::overflow_error>("BitvToInt: overflow");
      }
      if (fon9_LIKELY(type == fon9_BitvT_IntegerNeg)) {
         if (std::is_unsigned<T>::value)
            Raise<BitvSignedError>("BitvToInt: cannot load BitvNeg to unsigned");
         goto __POP_INT_VALUE;
      }
      if (*ptype == BitvV_0) {
         buf.PopConsumed(1u);
         return static_cast<T>(0);
      }
      if (*ptype == BitvV_Null) {
         buf.PopConsumed(1u);
         return nullValue;
      }
      Raise<BitvTypeNotMatch>("BitvToInt: type not match");
   }
   Raise<BitvNeedsMore>("BitvToInt: needs more");
}
   
fon9_API int8_t BitvToInt(DcQueue& buf, int8_t nullValue);
fon9_API uint8_t BitvToInt(DcQueue& buf, uint8_t nullValue);
fon9_API int16_t BitvToInt(DcQueue& buf, int16_t nullValue);
fon9_API uint16_t BitvToInt(DcQueue& buf, uint16_t nullValue);
fon9_API int32_t BitvToInt(DcQueue& buf, int32_t nullValue);
fon9_API uint32_t BitvToInt(DcQueue& buf, uint32_t nullValue);
fon9_API int64_t BitvToInt(DcQueue& buf, int64_t nullValue);
fon9_API uint64_t BitvToInt(DcQueue& buf, uint64_t nullValue);

template <class T>
inline enable_if_t<std::is_integral<T>::value>
BitvTo(DcQueue& buf, T& value) {
   value = BitvToInt(buf, value);
}

template <typename EnumT>
inline enable_if_t<std::is_enum<EnumT>::value, EnumT>
BitvToEnum(DcQueue& buf, EnumT nullValue) {
   auto v = static_cast<typename std::underlying_type<EnumT>::type>(nullValue);
   BitvTo(buf, v);
   return static_cast<EnumT>(v);
}

template <typename EnumT>
inline enable_if_t<std::is_enum<EnumT>::value>
BitvTo(DcQueue& buf, EnumT& value) {
   value = BitvToEnum(buf, value);
}

//--------------------------------------------------------------------------//

fon9_API void BitvToNumber(DcQueue& buf, fon9_BitvNumR& numr);

template <typename IntTypeT, DecScaleT ScaleN, class AdjustDecScaleEx = AdjustDecScaleExDefault>
inline void BitvToDec(DcQueue& buf, Decimal<IntTypeT, ScaleN>& value, AdjustDecScaleEx&& ex = AdjustDecScaleEx{}) {
   fon9_BitvNumR numr;
   BitvToNumber(buf, numr);
   switch (numr.Type_) {
   case fon9_BitvNumT_Null:
      value.AssignNull();
      break;
   case fon9_BitvNumT_Zero:
      value.Assign0();
      break;
   case fon9_BitvNumT_Neg:
      if (std::is_unsigned<IntTypeT>::value)
         Raise<BitvSignedError>("BitvToDec: cannot load BitvNeg to unsigned");
      value.Assign(static_cast<intmax_t>(numr.Num_), numr.Scale_, std::forward<AdjustDecScaleEx>(ex));
      break;
   case fon9_BitvNumT_Pos:
      value.Assign(numr.Num_, numr.Scale_, std::forward<AdjustDecScaleEx>(ex));
      break;
   }
}

template <typename IntTypeT, DecScaleT ScaleN, class AdjustDecScaleEx = AdjustDecScaleExDefault>
inline void BitvTo(DcQueue& buf, Decimal<IntTypeT, ScaleN>& value, AdjustDecScaleEx&& ex = AdjustDecScaleEx{}) {
   BitvToDec(buf, value, std::forward<AdjustDecScaleEx>(ex));
}

//--------------------------------------------------------------------------//

template <class Container>
struct IsInsertable {
   template <class C>
   static auto TestHasInsert(C& c) -> decltype(c.insert(c.end(), *c.begin()), bool());

   template <class C>
   static void TestHasInsert(...);

   enum { value = std::is_same<decltype(TestHasInsert<Container>(*static_cast<Container*>(nullptr))), bool>::value };
};

template <class Interator>
void BitvToArray(DcQueue& buf, Interator pbeg, Interator pend) {
   while (pbeg != pend)
      BitvTo(buf, *pbeg++);
}

/// 固定大小的陣列: `T (&ary)[arysz];` 或 `std::array<T,arysz>;` 之類.
/// 直接取出 N 個元素, 不使用 fon9_BitvT_Container.
template <class Container>
auto BitvTo(DcQueue& buf, Container& c) -> enable_if_t<sizeof(*std::begin(c)) != 1 && !IsInsertable<Container>::value> {
   BitvToArray(buf, std::begin(c), std::end(c));
}

//--------------------------------------------------------------------------//

fon9_API size_t BitvToContainerElementCount(DcQueue& buf);

template <class Container>
inline auto ContainerReserve(Container& c, size_t sz) -> decltype(c.reserve(sz), void()) {
   c.reserve(sz);
}
inline void ContainerReserve(...) {
}

template <class Key, class Value>
std::pair<const Key, Value> BitvToValue(DcQueue& buf, std::pair<const Key, Value>*) {
   Key   key;
   Value value{};
   BitvTo(buf, key);
   BitvTo(buf, value);
   return std::pair<const Key, Value>{std::move(key), std::move(value)};
}

template <class Container>
auto BitvToContainer(DcQueue& buf, Container& c, size_t sz)
-> decltype(BitvToValue(buf, &*c.begin()), void()) {
   if (sz > 0) {
      do {
         c.insert(c.end(), BitvToValue(buf, static_cast<typename Container::value_type*>(nullptr)));
      } while (--sz > 0);
   }
}

template <class Container>
auto BitvToContainer(DcQueue& buf, Container& c, size_t sz)
-> decltype(BitvTo(buf, *c.begin()), void()) {
   if (sz > 0) {
      decay_t<decltype(*c.begin())> v{};
      do {
         BitvTo(buf, v);
         c.insert(c.end(), std::move(v));
      } while (--sz > 0);
   }
}

template <class Container>
auto BitvTo(DcQueue& buf, Container& c)
-> enable_if_t<sizeof(*c.begin()) != 1, decltype(c.clear(), c.insert(c.end(), *c.begin()), void())> {
   c.clear();
   if (size_t sz = BitvToContainerElementCount(buf)) {
      ContainerReserve(c, sz);
      BitvToContainer(buf, c, sz);
   }
}

} // namespace
#endif//__fon9_BitvDecode_hpp__
