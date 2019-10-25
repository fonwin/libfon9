/// \file fon9/IntSel.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_IntSel_hpp__
#define __fon9_IntSel_hpp__
#include "fon9/Utility.hpp"

namespace fon9 {

/// \ingroup Misc
/// kNBytes = 儲存一個整數所需的bytes數量(1..8).
/// UIntTypeSelector<>::Type 定義 kNBytes 需要用哪種整數(uint8_t, uint16_t...)
template <size_t kNBytes> struct UIntTypeSelector;
template <> struct UIntTypeSelector<1> { using Type = uint8_t; };
template <> struct UIntTypeSelector<2> { using Type = uint16_t; };
template <> struct UIntTypeSelector<3> { using Type = uint32_t; };
template <> struct UIntTypeSelector<4> { using Type = uint32_t; };
template <> struct UIntTypeSelector<5> { using Type = uint64_t; };
template <> struct UIntTypeSelector<6> { using Type = uint64_t; };
template <> struct UIntTypeSelector<7> { using Type = uint64_t; };
template <> struct UIntTypeSelector<8> { using Type = uint64_t; };
template <size_t kNBytes>
using UIntTypeSelector_t = typename UIntTypeSelector<kNBytes>::Type;

template <size_t kNBytes> struct SIntTypeSelector;
template <> struct SIntTypeSelector<1> { using Type = int8_t; };
template <> struct SIntTypeSelector<2> { using Type = int16_t; };
template <> struct SIntTypeSelector<3> { using Type = int32_t; };
template <> struct SIntTypeSelector<4> { using Type = int32_t; };
template <> struct SIntTypeSelector<5> { using Type = int64_t; };
template <> struct SIntTypeSelector<6> { using Type = int64_t; };
template <> struct SIntTypeSelector<7> { using Type = int64_t; };
template <> struct SIntTypeSelector<8> { using Type = int64_t; };
template <size_t kNBytes>
using SIntTypeSelector_t = typename SIntTypeSelector<kNBytes>::Type;

template <size_t kNBytes, bool isSigned>
using IntTypeSelector = conditional_t<isSigned, SIntTypeSelector<kNBytes>, UIntTypeSelector<kNBytes>>;

//--------------------------------------------------------------------------//

/// \ingroup Misc
/// 選擇要能儲存最大整數 kMaxVal, 所須使用的整數型別(uint8_t, uint16_t...).
template <size_t kMaxVal, size_t kNBytes> struct LenTypeSelector;

template <size_t kNBytes>
struct LenTypeSelector<0, kNBytes> : UIntTypeSelector<kNBytes> {};

template <size_t kMaxVal, size_t kNBytes = 0>
struct LenTypeSelector : public LenTypeSelector<kMaxVal / 0x100, kNBytes + 1> {};

template <size_t kMaxVal, size_t kNBytes = 0>
using LenTypeSelector_t = typename LenTypeSelector<kMaxVal, kNBytes>::Type;

static_assert(sizeof(LenTypeSelector_t<1>) == 1, "");
static_assert(sizeof(LenTypeSelector_t<255>) == 1, "");
static_assert(sizeof(LenTypeSelector_t<256>) == 2, "");
static_assert(sizeof(LenTypeSelector_t<65535>) == 2, "");
static_assert(sizeof(LenTypeSelector_t<65536>) == 4, "");

} // namespace
#endif//__fon9_IntSel_hpp__
