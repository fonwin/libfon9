// \file f9twf/ExgMdSymbs.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdSymbs_hpp__
#define __f9twf_ExgMdSymbs_hpp__
#include "f9twf/ExgMdContracts.hpp"
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/fmkt/SymbTwf.hpp"
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/fmkt/SymbTimePri.hpp"
#include "fon9/fmkt/SymbBreakSt.hpp"
#include "fon9/fmkt/SymbFuoClosing.hpp"
#include "fon9/fmkt/SymbDynBand.hpp"
#include "fon9/fmkt/SymbQuoteReq.hpp"
#include "fon9/fmkt/MdRtStream.hpp"

namespace f9twf {

/// 檢查並設定 symb 的 TradingSessionId.
/// \retval true 應繼續處理 rxTradingSessionId 的其他資料.
///   - 現在的 symb.TradingSession_ 正確: symb.TradingSessionId_ == rxTradingSessionId;
///   - 設定了新的 rxTradingSessionId (例: 日盤 => 夜盤).
///   - 返回後, 必定 symb.TradingSessionId_ == rxTradingSessionId;
/// \retval false 應拋棄 rxTradingSessionId 的其他資料.
///   - 現在 symb 所在的盤別較新, 不變動 symb.TradingSessionId_;
inline bool CheckSymbTradingSessionId(fon9::fmkt::SymbTree& tree, fon9::fmkt::Symb& symb,
                                      f9fmkt_TradingSessionId rxTradingSessionId) {
   if (fon9_LIKELY(symb.TradingSessionId_ == rxTradingSessionId))
      return true;
   if (symb.TradingSessionId_ == f9fmkt_TradingSessionId_AfterHour)
      return false;
   symb.SessionClear(tree, rxTradingSessionId);
   return true;
}
//--------------------------------------------------------------------------//
/// TDayYYYYMMDD_ + TradingSessionId_ 用來判斷資料的新舊.
/// - 無法使用 I011.END-SESSION 來判斷商品的日夜盤狀態, 因為:
///   - 日盤的 I011.END-SESSION 永遠是 '0'一般交易時段;
///   - 夜盤的 I011.END-SESSION 永遠是 '1'盤後交易時段;
/// - 因此必須在收到資料時判斷: 如果已進入夜盤, 則應拋棄日盤的資料.
/// - 在 DailyClear(tdayYYYYMMDD) 時, 會設定 this->TDayYYYYMMDD_ = tdayYYYYMMDD;
/// - 在收到夜盤資料時會設定 this->TradingSessionId_ = f9fmkt_TradingSessionId_AfterHour;
/// - 可使用 this->CheckSetTradingSessionId() 來檢查並設定 TradingSessionId.
class f9twf_API ExgMdSymb : public fon9::fmkt::SymbTwf, public fon9::fmkt::SymbDataOHL {
   fon9_NON_COPY_NON_MOVE(ExgMdSymb);
   using base = fon9::fmkt::SymbTwf;
public:
   TwfSymbRef                 Ref_;
   fon9::fmkt::SymbTwfBS      BS_;
   fon9::fmkt::SymbDeal       Deal_;
   fon9::fmkt::SymbBreakSt    BreakSt_;
   fon9::fmkt::SymbFuoClosing FuoClosing_;
   fon9::fmkt::SymbDynBand    DynBand_;
   fon9::fmkt::SymbQuoteReq   QuoteReq_;
   fon9::fmkt::MdRtStream     MdRtStream_;
   ExgMdContract&             Contract_;

   ExgMdSymb(const fon9::StrView& symbid, ExgMdSymbs& owner);
   ~ExgMdSymb();

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

   void SessionClear(fon9::fmkt::SymbTree& owner, f9fmkt_TradingSessionId tsesId) override;
   void DailyClear(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) override;

   /// 移除商品, 通常是因為商品下市, 預設觸發:
   /// - this->MdRtStream_.BeforeRemove(owner, *this);
   /// - this->Contract_->OnSymbRemove(*this);
   void OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) override;

   static fon9::seed::LayoutSP MakeLayout(bool isAddMarketSeq);
};
//--------------------------------------------------------------------------//
class f9twf_API ExgMdSymbs : public fon9::fmkt::MdSymbsT<ExgMdSymb> {
   fon9_NON_COPY_NON_MOVE(ExgMdSymbs);
   using base = fon9::fmkt::MdSymbsT<ExgMdSymb>;
   ExgMdContracts Contracts_;

   void OnAfterLoadFrom(Locker&& symbsLk) override;

public:
   ExgMdSymbs(std::string rtiPathFmt, bool isAddMarketSeq);

   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;

   ExgMdContract& FetchContract(ExgMdSymb& symb) {
      assert(this->SymbMap_.IsLocked());
      return this->Contracts_.FetchContract(symb);
   }
   ExgMdContract& FetchContract(ContractId conId) {
      assert(this->SymbMap_.IsLocked());
      return this->Contracts_.FetchContract(conId);
   }
   ExgMdContract& FetchContract(fon9::StrView symbid) {
      assert(this->SymbMap_.IsLocked());
      return this->Contracts_.FetchContract(symbid);
   }
   const ExgMdContracts& GetContracts(Locker&) const {
      return this->Contracts_;
   }

   struct MdTreeOp : public MdSymbsOp {
      fon9_NON_COPY_NON_MOVE(MdTreeOp);
      using MdSymbsOp::MdSymbsOp;
      void Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override;
   };
   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override;
};
using ExgMdSymbsSP = fon9::intrusive_ptr<ExgMdSymbs>;
//--------------------------------------------------------------------------//
struct ExgMdEntry;

/// 返回 mdEntry + mdCount;
f9twf_API const void* ExgMdToSnapshotBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry,
                                        fon9::fmkt::SymbTwfBS& symbBS, uint32_t priceOrigDiv, fon9::fmkt::MarketDataSeq mktseq);
template <class SymbT>
inline const void* ExgMdToSnapshotBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry, SymbT& symb, fon9::fmkt::MarketDataSeq mktseq) {
   return ExgMdToSnapshotBS(mdTime, mdCount, mdEntry, symb.BS_, symb.PriceOrigDiv_, mktseq);
}
//----------------------
struct ExgMcI081Entry;

f9twf_API void ExgMdToUpdateBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry,
                               fon9::fmkt::SymbTwfBS& symbBS, uint32_t priceOrigDiv, fon9::fmkt::MarketDataSeq mktseq);
template <class SymbT>
inline void ExgMdToUpdateBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry, SymbT& symb, fon9::fmkt::MarketDataSeq mktseq) {
   ExgMdToUpdateBS(mdTime, mdCount, mdEntry, symb.BS_, symb.PriceOrigDiv_, mktseq);
}

} // namespaces
#endif//__f9twf_ExgMdSymbs_hpp__
