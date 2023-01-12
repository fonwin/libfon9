// \file fon9/fmkt/FmdTypes.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmdTypes_hpp__
#define __fon9_fmkt_FmdTypes_hpp__
#include "fon9/fmkt/FmdTypes.h"
#include "fon9/PackBcd.hpp"
#include "fon9/BitvEncode.hpp"

fon9_ENABLE_ENUM_BITWISE_OP(f9sv_BSFlag);
fon9_ENABLE_ENUM_BITWISE_OP(f9sv_DealFlag);

namespace fon9 { namespace fmkt {

using MarketDataSeq = uint32_t;

/// 目前「證交所、期交所」的行情封包序號 = PackBcd<10>;
inline MarketDataSeq PackBcdToMarketSeq(const PackBcd<10> &pbcd) {
   return PackBcdTo<sizeof(pbcd) * 2, MarketDataSeq>(pbcd);
}

enum class MdSymbsCtrlFlag {
   /// 允許訂閱整棵樹.
   AllowSubrTree = 0x01,
   /// 訂閱整棵樹時, 允許取得當時全部商品的快照.
   /// 因為要打包商品的快照 SnapshotSymb: Base、Ref、High、Low、BS、Deal...
   /// 所以會瞬間占用大量資源, 且會瞬間造成網路流量大增;
   AllowSubrSnapshotSymb = 0x02 | AllowSubrTree,
   /// 是否有提供 MarketSeq 欄位?
   HasMarketDataSeq = 0x10,
   /// 是否有提供 ChannelSeq 欄位?
   HasChannelSeq = 0x20,
};
fon9_ENABLE_ENUM_BITWISE_OP(MdSymbsCtrlFlag);

inline void PackMktSeq(RevBuffer& rbuf, MarketDataSeq& dst, MdSymbsCtrlFlag flags, const PackBcd<10> &pbcd) {
   if (IsEnumContains(flags, MdSymbsCtrlFlag::HasMarketDataSeq))
      ToBitv(rbuf, dst = PackBcdToMarketSeq(pbcd));
}
inline void PackMktSeq(RevBuffer& rbuf, MarketDataSeq& dst, MdSymbsCtrlFlag flags, MarketDataSeq seq) {
   if (IsEnumContains(flags, MdSymbsCtrlFlag::HasMarketDataSeq))
      ToBitv(rbuf, dst = seq);
}
inline void PackMktSeq(RevBuffer& rbuf, MdSymbsCtrlFlag flags, MarketDataSeq seq) {
   if (IsEnumContains(flags, MdSymbsCtrlFlag::HasMarketDataSeq))
      ToBitv(rbuf, seq);
}

} } // namespaces
#endif//__fon9_fmkt_FmdTypes_hpp__
