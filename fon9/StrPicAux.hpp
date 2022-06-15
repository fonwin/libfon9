/// \file fon9/StrPicAux.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_StrPicAux_hpp__
#define __fon9_StrPicAux_hpp__
#include "fon9/ToStr.hpp"
#include "fon9/CharAry.hpp"
#include "fon9/Decimal.hpp"

namespace fon9 {

/// \ingroup AlNum
/// 從 *psrc 開始, 複製到 *pdst, 共複製 size 個 char;
/// 若 psrc 尾端遇到 kChSrcFiller 則改成 kChDstRightPad;
template <size_t size, char kChDstRightPad, char kChSrcFiller>
static inline void LCopyPicX(char* pdst, const char* psrc) {
   memcpy(pdst, psrc, size);
   pdst += size;
   while (*--pdst == kChSrcFiller) {
      *pdst = kChDstRightPad;
   }
}
/// \ingroup AlNum
/// 若 src 尾端遇到 kChSrcRightPad, 則複製 kChDstFiller;
template <char kChSrcRightPad, typename CharT, size_t kSize, CharT kChDstFiller, CharT kChSrcFiller>
static inline void LCopyFromPicX(CharAry<kSize, CharT, kChDstFiller>& dst,
                                 const CharAry<kSize, CharT, kChSrcFiller>& src) {
   LCopyPicX<kSize, kChDstFiller, kChSrcRightPad>(dst.begin(), src.begin());
}
/// \ingroup AlNum
/// 若 src 尾端遇到 kChSrcFiller, 則複製 kChDstRightPad;
template <char kChDstRightPad, typename CharT, size_t kDstSize, CharT kChDstFiller, size_t kSrcSize, CharT kChSrcFiller>
static inline void LCopyToPicX(CharAry<kDstSize, CharT, kChDstFiller>& dst,
                               const CharAry<kSrcSize, CharT, kChSrcFiller>& src) {
   static_assert(kDstSize <= kSrcSize, "");
   LCopyPicX<kDstSize, kChDstRightPad, kChSrcFiller>(dst.begin(), src.begin());
}

/// \ingroup AlNum
/// 從 *psrc 開始, 複製到 *dst.begin(); 最多複製 arysz 個 char;
/// 若 *psrc 尾端遇到 kChSrcFiller 則不複製; 然後在 pdst 左方填入 kChDstLeftPad;
/// 例如:
///   size=5; kChDstLeftPad='0'; kChSrcFiller='-';
///   psrc="123--"; 則 pdst 會填入: "00123";
template <size_t size, char kChDstLeftPad, char kChSrcFiller>
static inline void RCopyPicX(char* pdst, const char* psrc) {
   psrc += size;
   unsigned L = size;
   while (L > 0) {
      if (*--psrc == kChSrcFiller) {
         *pdst++ = kChDstLeftPad;
         --L;
         continue;
      }
      pdst += L;
      do {
         *--pdst = *psrc--;
      } while (--L > 0);
      break;
   }
}
template <char kChDstLeftPad, typename CharT,
   size_t kSrcSize, CharT kChSrcFiller,
   size_t kDstSize, CharT kChDstFiller
>
inline void RCopyToPicX(CharAry<kDstSize, CharT, kChDstFiller>& dst,
                        const CharAry<kSrcSize, CharT, kChSrcFiller>& src) {
   static_assert(kDstSize <= kSrcSize, "");
   RCopyPicX<kDstSize, kChDstLeftPad, kChSrcFiller>(dst.begin(), src.begin());
}

//--------------------------------------------------------------------------//

template <size_t Width, typename IntT, char kChFiller>
static inline auto UIntToPic9(CharAry<Width, char, kChFiller>& dst, IntT value) -> enable_if_t<std::is_unsigned<IntT>::value> {
   impl::AuxPic9ToStr<IntT, Width>::ToStr(dst.end(), value);
}
template <size_t Width, typename IntT, char kChFiller>
static inline auto SIntToPic9(CharAry<Width, char, kChFiller>& dst, IntT value) -> enable_if_t<std::is_signed<IntT>::value> {
   SPic9ToStrRev<Width - 1>(dst.end(), value);
}

static inline void UPriToPic9_5v4(CharAry<9>& dst, Decimal<uint32_t, 4> pri) {
   static_assert(pri.Scale == 4, "");
   UIntToPic9(dst, pri.GetOrigValue());
}

} // namespace fon9
#endif//__fon9_StrPicAux_hpp__
