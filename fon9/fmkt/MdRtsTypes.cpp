// \file fon9/fmkt/MdRtsTypes.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/BitvEncode.hpp"

namespace fon9 { namespace fmkt {

fon9_API f9sv_MdRtsKind GetMdRtsKind(f9sv_RtsPackType pkType) {
   static constexpr f9sv_MdRtsKind RtsPackType2MdRtsKind[] = {
      f9sv_MdRtsKind_Deal,
      f9sv_MdRtsKind_BS,
      f9sv_MdRtsKind_BS,
      f9sv_MdRtsKind_BS,
      f9sv_MdRtsKind_TradingSession,
      f9sv_MdRtsKind_Base | f9sv_MdRtsKind_Ref,
      f9sv_MdRtsKind_Deal | f9sv_MdRtsKind_BS,
      f9sv_MdRtsKind_All,   // SnapshotSymbList_NoInfoTime
      f9sv_MdRtsKind_Deal,  // IndexValueV2
      f9sv_MdRtsKind_Deal,  // IndexValueV2List
      f9sv_MdRtsKind_All,   // FieldValue_NoInfoTime: 因為不確定要更新哪個欄位, 所以只好視為全部都有可能.
      f9sv_MdRtsKind_All - f9sv_MdRtsKind_NoInfoTime,
      f9sv_MdRtsKind_All,   // TabValues_NoInfoTime:  因為不確定要更新哪個Tab, 所以只好視為全部都有可能.
      f9sv_MdRtsKind_All - f9sv_MdRtsKind_NoInfoTime,
   };
   static_assert(numofele(RtsPackType2MdRtsKind) == cast_to_underlying(f9sv_RtsPackType_Count), "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_DealPack)] == f9sv_MdRtsKind_Deal, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_SnapshotBS)] == f9sv_MdRtsKind_BS, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_CalculatedBS)] == f9sv_MdRtsKind_BS, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_UpdateBS)] == f9sv_MdRtsKind_BS, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_TradingSessionId)] == f9sv_MdRtsKind_TradingSession, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_BaseInfoTw)] == (f9sv_MdRtsKind_Base | f9sv_MdRtsKind_Ref), "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_DealBS)] == (f9sv_MdRtsKind_Deal | f9sv_MdRtsKind_BS), "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_SnapshotSymbList_NoInfoTime)] == f9sv_MdRtsKind_All, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_IndexValueV2)] == f9sv_MdRtsKind_Deal, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_IndexValueV2List)] == f9sv_MdRtsKind_Deal, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_FieldValue_NoInfoTime)] == f9sv_MdRtsKind_All, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_FieldValue_AndInfoTime)] == f9sv_MdRtsKind_All - f9sv_MdRtsKind_NoInfoTime, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_TabValues_NoInfoTime)] == f9sv_MdRtsKind_All, "");
   static_assert(RtsPackType2MdRtsKind[cast_to_underlying(f9sv_RtsPackType_TabValues_AndInfoTime)] == f9sv_MdRtsKind_All - f9sv_MdRtsKind_NoInfoTime, "");

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
