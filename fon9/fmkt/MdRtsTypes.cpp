// \file fon9/fmkt/MdRtsTypes.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/BitvEncode.hpp"

namespace fon9 { namespace fmkt {

fon9_API MdRtsKind GetMdRtsKind(RtsPackType pkType) {
   static constexpr MdRtsKind RtsPackType2MdRtsKind[] = {
      MdRtsKind::Deal,
      MdRtsKind::BS,
      MdRtsKind::BS,
      MdRtsKind::BS,
      MdRtsKind::TradingSession,
      MdRtsKind::Base | MdRtsKind::Ref,
      MdRtsKind::Deal | MdRtsKind::BS,
      MdRtsKind::All,   // SnapshotSymbList_NoInfoTime
      MdRtsKind::Deal,  // IndexValueV2
      MdRtsKind::Deal,  // IndexValueV2List
      MdRtsKind::All,   // FieldValue_NoInfoTime: 因為不確定要更新哪個欄位, 所以只好視為全部都有可能.
      MdRtsKind::All - MdRtsKind::NoInfoTime,
      MdRtsKind::All,   // TabValues_NoInfoTime:  因為不確定要更新哪個Tab, 所以只好視為全部都有可能.
      MdRtsKind::All - MdRtsKind::NoInfoTime,
   };
   static_assert(numofele(RtsPackType2MdRtsKind) == cast_to_underlying(RtsPackType::Count), "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::DealPack)] == MdRtsKind::Deal, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::SnapshotBS)] == MdRtsKind::BS, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::CalculatedBS)] == MdRtsKind::BS, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::UpdateBS)] == MdRtsKind::BS, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::TradingSessionId)] == MdRtsKind::TradingSession, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::BaseInfoTw)] == (MdRtsKind::Base | MdRtsKind::Ref), "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::DealBS)] == (MdRtsKind::Deal | MdRtsKind::BS), "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::SnapshotSymbList_NoInfoTime)] == MdRtsKind::All, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::IndexValueV2)] == MdRtsKind::Deal, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::IndexValueV2List)] == MdRtsKind::Deal, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::FieldValue_NoInfoTime)] == MdRtsKind::All, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::FieldValue_AndInfoTime)] == MdRtsKind::All - MdRtsKind::NoInfoTime, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::TabValues_NoInfoTime)] == MdRtsKind::All, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(RtsPackType::TabValues_AndInfoTime)] == MdRtsKind::All - MdRtsKind::NoInfoTime, "");

   return RtsPackType2MdRtsKind[cast_to_underlying(pkType)];
}
//--------------------------------------------------------------------------//
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
   if (IsEnumContains(curFlags, BSFlag::DerivedSell))
      MdRtsPackSingleBS(rbuf, RtBSType::DerivedSell, static_cast<const SymbTwfBSData*>(&symbBS)->DerivedSell_);
   if (IsEnumContains(curFlags, BSFlag::DerivedBuy))
      MdRtsPackSingleBS(rbuf, RtBSType::DerivedBuy, static_cast<const SymbTwfBSData*>(&symbBS)->DerivedBuy_);
   if (IsEnumContains(curFlags, BSFlag::OrderSell))
      MdRtsPackOrderBS(rbuf, RtBSType::OrderSell, symbBS.Sells_);
   if (IsEnumContains(curFlags, BSFlag::OrderBuy))
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
