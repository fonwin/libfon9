// \file fon9/fmkt/MdRtsTypes.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/BitvEncode.hpp"

namespace fon9 { namespace fmkt {

static bool MdRtsPackSingleBS(RevBuffer& rbuf, RtBSType bsType, const PriQty& pq) {
   if (pq.Qty_ == 0)
      return false;
   ToBitv(rbuf, pq.Qty_);
   ToBitv(rbuf, pq.Pri_);
   *rbuf.AllocPacket<RtBSType>() = bsType;
   static_assert(sizeof(RtBSType) == 1, "");
   return true;
}

/// 將委託簿「買方 or 賣方」的 pqs[SymbBS::kBSCount] 填入 rbuf;
static bool MdRtsPackOrderBS(RevBuffer& rbuf, RtBSType bsType, const PriQty* pqs) {
   unsigned bsCount = 0;
   for (bsCount = 0; bsCount < SymbBSData::kBSCount; ++bsCount) {
      if (pqs->Qty_ == 0)
         break;
      ToBitv(rbuf, pqs->Qty_);
      ToBitv(rbuf, pqs->Pri_);
      ++pqs;
   }
   if (bsCount == 0)
      return false;
   *rbuf.AllocPacket<uint8_t>() = static_cast<uint8_t>(cast_to_underlying(bsType) | (bsCount - 1));
   static_assert(sizeof(bsType) == 1, "");
   return true;
}

fon9_API void MdRtsPackSnapshotBS(RevBuffer& rbuf, const SymbBSData& symbBS) {
   const auto  curFlags = symbBS.Flags_;
   if (IsEnumContains(curFlags, f9sv_BSFlag_DerivedSell))
      MdRtsPackSingleBS(rbuf, RtBSType::DerivedSell, static_cast<const SymbTwfBSData*>(&symbBS)->DerivedSell_);
   if (IsEnumContains(curFlags, f9sv_BSFlag_DerivedBuy))
      MdRtsPackSingleBS(rbuf, RtBSType::DerivedBuy, static_cast<const SymbTwfBSData*>(&symbBS)->DerivedBuy_);
   if (IsEnumContains(curFlags, f9sv_BSFlag_OrderSell))
      MdRtsPackOrderBS(rbuf, RtBSType::OrderSell, symbBS.Sells_);
   if (IsEnumContains(curFlags, f9sv_BSFlag_OrderBuy))
      MdRtsPackOrderBS(rbuf, RtBSType::OrderBuy, symbBS.Buys_);
}

fon9_API void MdRtsPackTabValues(RevBuffer& rbuf, const seed::Tab& tab, const SymbData& dat) {
   assert(unsigned_cast(tab.GetIndex()) <= 0xff);
   seed::SimpleRawRd rd{dat};
   auto fldidx = tab.Fields_.size();
   while (fldidx > 0)
      tab.Fields_.Get(--fldidx)->CellToBitv(rd, rbuf);
   *rbuf.AllocPacket<uint8_t>() = static_cast<uint8_t>(tab.GetIndex());
}

} } // namespaces
