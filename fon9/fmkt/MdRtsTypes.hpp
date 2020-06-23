// \file fon9/fmkt/MdRtsTypes.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdRtsTypes_hpp__
#define __fon9_fmkt_MdRtsTypes_hpp__
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/fmkt/FmdRtsPackType.h"
#include "fon9/seed/Tab.hpp"
#include "fon9/TimeStamp.hpp"

fon9_ENABLE_ENUM_BITWISE_OP(f9sv_MdRtsKind);

namespace fon9 { namespace fmkt {

static_assert(cast_to_underlying(f9sv_RtsPackType_Count) <= 0x7f, "");

inline f9sv_MdRtsKind StrToMdRtsKind(StrView* args) {
   f9sv_MdRtsKind res = static_cast<f9sv_MdRtsKind>(HexStrTo(args));
   return(res == f9sv_MdRtsKind{} ? f9sv_MdRtsKind_All : res);
}
fon9_API f9sv_MdRtsKind GetMdRtsKind(f9sv_RtsPackType pkType);
//--------------------------------------------------------------------------//
/// static_assert(ClosedIndexTime().GetOrigValue() == 99 * 24 * 60 * 60 * DayTime::Divisor, "");
/// = 8553600000000
constexpr DayTime ClosedIndexTime() {
   return fon9::TimeInterval_Day(99);
}
/// static_assert(YesterdayIndexTime().GetOrigValue() == -1 * 24 * 60 * 60 * DayTime::Divisor, "");
/// = -86400000000
constexpr DayTime YesterdayIndexTime() {
   return fon9::TimeInterval_Day(-1);
}
//--------------------------------------------------------------------------//
// MSB 必須為 1, 也就是 RtBSSnapshotSpc 必須 >= 0x80;
enum class RtBSSnapshotSpc : uint8_t {
   LmtFlags = 0x80
};
enum class RtBSType : uint8_t {
   // --xx ----
   Mask = 0x30,
   OrderBuy = 0x00,
   OrderSell = 0x10,
   DerivedBuy = 0x20,
   DerivedSell = 0x30,
};
enum class RtBSAction : uint8_t {
   // xx-- ----
   Mask = 0xc0,
   New = 0x00,
   Delete = 0x40,
   ChangePQ = 0x80,
   ChangeQty = 0xc0,
};
//--------------------------------------------------------------------------//
class SymbData;
struct SymbBSData;

fon9_API void MdRtsPackSnapshotBS(RevBuffer& rbuf, const SymbBSData& symbBS);

/// 用 f9sv_RtsPackType_TabValues 格式打包 dat.
/// - 「不包含」最後的 *rbuf.AllocPacket<uint8_t>() = cast_to_underlying(f9sv_RtsPackType_TabValues);
/// - 可以呼叫多次 MdRtsPackTabValues() 打包不同的 tab;
fon9_API void MdRtsPackTabValues(RevBuffer& rbuf, const seed::Tab& tab, const SymbData& dat);

inline void MdRtsPackFieldValueNid(RevBuffer& rbuf, const seed::Tab& tab, const seed::Field& fld) {
   assert(unsigned_cast(tab.GetIndex()) <= 0x0f && unsigned_cast(fld.GetIndex()) <= 0x0f);
   *rbuf.AllocPacket<uint8_t>() = static_cast<uint8_t>((tab.GetIndex() << 4) | fld.GetIndex());
}
inline void MdRtsPackFieldValue(RevBuffer& rbuf,
                                const seed::Tab& tab,
                                const seed::Field& fld,
                                const seed::RawRd& rd) {
   fld.CellToBitv(rd, rbuf);
   MdRtsPackFieldValueNid(rbuf, tab, fld);
}

} } // namespaces
#endif//__fon9_fmkt_MdRtsTypes_hpp__
