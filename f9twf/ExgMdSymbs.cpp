// \file f9twf/ExgMdSymbs.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdSymbs.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9twf {

fon9::seed::LayoutSP ExgMdSymb::MakeLayout() {
   using namespace fon9::seed;
   using namespace fon9::fmkt;
   // -----
   // TODO:
   // -----
   // [Tab:Ref] Add fields:
   // - PriUpLmt2_; PriUpLmt3_; int8_t LvUpLmt_; // =0使用PriUpLmt_, <0(-2,-3)預告, >0(2,3)已實施.
   //   PriDnLmt2_; PriDnLmt3_; int8_t LvDnLmt_;
   // - 暫停(恢復)交易: BreakTime_, RestartTime_, ReopenTime_; Reason_;
   // - TradingSessionSt = 參照流程組 or 開始收單, 開盤, 不可刪單階段 Non-cancel Period;
   // - 也許要增加 flag: 告知最後異動的欄位;
   // -----
   // Add tree:「流程群組狀態tree」, 可訂閱tree, 期貨、選擇權各一個 tree;
   // - I140; 市場狀態通知
   //   - TradingSessionSt = 開始收單, 開盤, 不可刪單階段 Non-cancel Period, 收盤或不再恢復交易;
   //   - Reason;
   // -----
   // Add [Tab:價穩措施]:
   // I010: DYNAMIC BANDING: 商品是否適用動態價格穩定
   // I140: Unknown, 暫停、恢復、調整範圍
   // -----
   // Add [Tab:詢價]:
   //    詢價揭示時間(DISCLOSURE TIME), 詢價持續時間(999 秒: DURATION TIME);
   // -----
   // Add [Tab:收盤後資料]:
   //    結算價(SETTLEMENT PRICE);
   //    未平倉合約數(OPEN INTEREST);
   //    鉅額交易總成交量(BLOCK TRADE QTY);
   // -----
   // Add tree: Contract; from I011; I020;
   // 期貨價差複式單(例: TXFB0/C0), 沒有基本資料(I010)
   // 所以沒有 ExgMdSymb::PriceOrigDiv_;
   // 此時應該參考第1腳(或用前3碼取得 Contract:契約基本資料: I011).
   // ExgMdSymb 應增加 ContractSP Contract_; 並在 ExgMdSymb 建構時設定好;
   // - Contract:契約基本資料 應包含:
   //   - I011:
   //     - PriceOrigDiv_; StrikePriceDiv_;
   //     - CONTRACT-SIZE;
   //     - ACCEPT-QUOTE-FLAG;
   //   - I020:
   //     - INDEX-NUMBER 股票代碼;
   // -----
   // 新增解析的格式, 要在建立 ExgMcChannelMgr 之後, 透過 ExgMcChannelMgr::RegMcMessageParser() 註冊;
   // -----
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Base},MakeFields(),           TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Ref}, SymbRef::MakeFields(),  TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_BS},  SymbBS::MakeFields(),   TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Deal},SymbDeal::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_High},SymbHigh::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Low}, SymbLow::MakeFields(),  TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Rt},  MdRtStream::MakeFields(),TabFlag::NoSapling_NoSeedCommand_Writable}}
   )};
}
static const int32_t kExgMdSymbOffset[]{
   0, // Base
   fon9_OffsetOf(ExgMdSymb, Ref_),
   fon9_OffsetOf(ExgMdSymb, BS_),
   fon9_OffsetOf(ExgMdSymb, Deal_),
   fon9_OffsetOf(ExgMdSymb, High_),
   fon9_OffsetOf(ExgMdSymb, Low_),
   fon9_OffsetOf(ExgMdSymb, MdRtStream_),
};
static inline fon9::fmkt::SymbData* GetExgMdSymbData(ExgMdSymb* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kExgMdSymbOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kExgMdSymbOffset[tabid])
      : nullptr;
}
fon9::fmkt::SymbData* ExgMdSymb::GetSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
fon9::fmkt::SymbData* ExgMdSymb::FetchSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
//--------------------------------------------------------------------------//
void ExgMdSymb::SessionClear(fon9::fmkt::SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   base::SessionClear(owner, tsesId);
   this->Ref_.DailyClear();
   this->Deal_.DailyClear();
   this->BS_.DailyClear();
   this->High_.DailyClear();
   this->Low_.DailyClear();
   this->MdRtStream_.SessionClear(owner, *this);
}
void ExgMdSymb::OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(owner, *this);
}
//--------------------------------------------------------------------------//
fon9::fmkt::SymbSP ExgMdSymbs::MakeSymb(const fon9::StrView& symbid) {
   return new ExgMdSymb(symbid, this->RtInnMgr_);
}
//--------------------------------------------------------------------------//
f9twf_API const void* ExgMdToSnapshotBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry,
                                        fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv) {
   symbBS.Clear(mdTime);
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      switch (mdEntry->EntryType_) {
      case '0':
         if (lv >= symbBS.kBSCount)
            continue;
         dst = symbBS.Data_.Buys_ + lv;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderBuy;
         break;
      case '1':
         if (lv >= symbBS.kBSCount)
            continue;
         dst = symbBS.Data_.Sells_ + lv;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderSell;
         break;
      case 'E':
         dst = &symbBS.Data_.DerivedBuy_;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedBuy;
         break;
      case 'F':
         dst = &symbBS.Data_.DerivedSell_;
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedSell;
         break;
      default:
         continue;
      }
      mdEntry->Price_.AssignTo(dst->Pri_, priceOrigDiv);
      dst->Qty_ = fon9::PackBcdTo<uint32_t>(mdEntry->Qty_);
   }
   return mdEntry;
}
f9twf_API void ExgMdToUpdateBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry,
                               fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv) {
   symbBS.Data_.InfoTime_ = mdTime;
   symbBS.Data_.Flags_ = fon9::fmkt::BSFlag{};
   for (unsigned mdL = 0; mdL < mdCount; ++mdL, ++mdEntry) {
      unsigned lv = fon9::PackBcdTo<unsigned>(mdEntry->Level_) - 1;
      fon9::fmkt::PriQty* dst;
      unsigned dstCount;
      switch (mdEntry->EntryType_) {
      case '0':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderBuy;
         dst = symbBS.Data_.Buys_;
         dstCount = symbBS.kBSCount;
         break;
      case '1':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::OrderSell;
         dst = symbBS.Data_.Sells_;
         dstCount = symbBS.kBSCount;
         break;
      case 'E':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedBuy;
         dst = &symbBS.Data_.DerivedBuy_;
         dstCount = 1;
         break;
      case 'F':
         symbBS.Data_.Flags_ |= fon9::fmkt::BSFlag::DerivedSell;
         dst = &symbBS.Data_.DerivedSell_;
         dstCount = 1;
         break;
      default:
         continue;
      }
      if (lv >= dstCount)
         continue;
      dst += lv;
      switch (mdEntry->UpdateAction_) {
      case '2': // Delete.
         if (auto mvc = dstCount - lv - 1)
            memmove(dst, dst + 1, mvc * sizeof(*dst));
         memset(dst + dstCount - lv - 1, 0, sizeof(*dst));
         break;
      case '0': // New.
         if (auto mvc = dstCount - lv - 1)
            memmove(dst + 1, dst, mvc * sizeof(*dst));
      case '1': // Change.
      case '5': // Overlay.
         mdEntry->Price_.AssignTo(dst->Pri_, priceOrigDiv);
         dst->Qty_ = fon9::PackBcdTo<uint32_t>(mdEntry->Qty_);
         break;
      default:
         continue;
      }
   }
}

} // namespaces
