// \file fon9/rc/RcMdRtsDecoder.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcMdRtsDecoder.hpp"
#include "fon9/rc/RcMdRtsDecoder.h"
#include "fon9/fmkt/SymbDealData.hpp"
#include "fon9/fmkt/SymbBSData.hpp"
#include "fon9/fmkt/SymbTabNames.h"
#include "fon9/seed/RawWr.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 { namespace rc {

#define STATIC_ASSERT_CHECK_ENUM(EnumTypeName, ValueName) \
   static_assert(cast_to_underlying(f9sv_##EnumTypeName##_##ValueName) == cast_to_underlying(fmkt::EnumTypeName::ValueName), "");
// -----
STATIC_ASSERT_CHECK_ENUM(DealFlag, Calculated); // f9sv_DealFlag_Calculated == fon9::fmkt::DealFlag::Calculated;
STATIC_ASSERT_CHECK_ENUM(DealFlag, DealTimeChanged);
STATIC_ASSERT_CHECK_ENUM(DealFlag, DealBuyCntChanged);
STATIC_ASSERT_CHECK_ENUM(DealFlag, DealSellCntChanged);
STATIC_ASSERT_CHECK_ENUM(DealFlag, TotalQtyLost);
STATIC_ASSERT_CHECK_ENUM(DealFlag, HeldMatchMask);
STATIC_ASSERT_CHECK_ENUM(DealFlag, HeldMatchTrendRise);
STATIC_ASSERT_CHECK_ENUM(DealFlag, HeldMatchTrendFall);

STATIC_ASSERT_CHECK_ENUM(BSFlag, Calculated); // f9sv_BSFlag_Calculated == fon9::fmkt::BSFlag::Calculated;
STATIC_ASSERT_CHECK_ENUM(BSFlag, OrderBuy);
STATIC_ASSERT_CHECK_ENUM(BSFlag, OrderSell);
STATIC_ASSERT_CHECK_ENUM(BSFlag, DerivedBuy);
STATIC_ASSERT_CHECK_ENUM(BSFlag, DerivedSell);
//--------------------------------------------------------------------------//
/// 讀取 BigEndian 的數值.
/// 若資料不足, 則觸發 exception: Raise<BitvNeedsMore>("RcMdRtsDecoder.Read");
template <typename ValueT>
inline ValueT ReadOrRaise(DcQueue& rxbuf) {
   ValueT res;
   auto*  pres = rxbuf.Peek(&res, sizeof(res));
   if(pres == nullptr)
      Raise<BitvNeedsMore>("RcMdRtsDecoder.Read");
   res = GetBigEndian<ValueT>(pres);
   rxbuf.PopConsumed(sizeof(ValueT));
   return res;
}
inline seed::Tab* GetTabOrRaise(seed::Layout& layout, StrView tabName) {
   if (auto* tab = layout.GetTab(tabName))
      return tab;
   Raise<std::runtime_error>(RevPrintTo<std::string>("MdRts:Not found tab=", tabName));
}
inline seed::Tab* GetTabOrNull(seed::Layout& layout, StrView tabName) {
   return layout.GetTab(tabName);
}
inline const seed::Field* GetFieldOrRaise(seed::Tab& tab, StrView fldName) {
   if (auto* fld = tab.Fields_.Get(fldName))
      return fld;
   Raise<std::runtime_error>(RevPrintTo<std::string>(
      "MdRts:Not found field=", fldName, "|tab=", tab.Name_));
}
inline const seed::Field* GetFieldOrNull(seed::Tab& tab, StrView fldName) {
   return tab.Fields_.Get(fldName);
}
/// 如果為 Null 表示: 不變動 dst;
static void BitvToDayTimeOrUnchange(DcQueue& rxbuf, DayTime& dst) {
   DayTime val{DayTime::Null()};
   BitvTo(rxbuf, val);
   if (!val.IsNull())
      dst = val;
}
//--------------------------------------------------------------------------//
class RcSvStreamDecoderNote_MdRts : public RcSvStreamDecoderNote {
   fon9_NON_COPY_NON_MOVE(RcSvStreamDecoderNote_MdRts);
   DayTime     RtInfoTime_{DayTime::Null()};
   DayTime     ReInfoTime_{DayTime::Null()};
   svc::PodRec RePod_;
   svc::SeedSP* GetRptSeedArray(svc::RxSubrData& rx) {
      if (rx.NotifyKind_ == seed::SeedNotifyKind::StreamData) {
         if (!rx.IsSubrTree_ || rx.SeedKey_.empty() || seed::IsSubrTree(rx.SeedKey_.begin()))
            return rx.SubrRec_->Seeds_;
         return rx.SubrRec_->Tree_->FetchPod(ToStrView(rx.SeedKey_)).Seeds_;
      }
      if (this->RePod_.Seeds_ == nullptr)
         rx.SubrRec_->Tree_->MakePod(this->RePod_);
      return this->RePod_.Seeds_;
   }
public:
   RcSvStreamDecoderNote_MdRts() = default;
   svc::SeedSP* SetRptSeedArray(f9sv_ClientReport& rpt, svc::RxSubrData& rx) {
      auto retval = this->GetRptSeedArray(rx);
      rpt.SeedArray_ = ToSeedArray(retval);
      return retval;
   }
   DayTime* SelectInfoTime(svc::RxSubrData& rx) {
      return (rx.NotifyKind_ == seed::SeedNotifyKind::StreamData
              ? &this->RtInfoTime_ : &this->ReInfoTime_);
   }
};

struct RcSvStreamDecoder_MdRts : public RcSvStreamDecoder {
   fon9_NON_COPY_NON_MOVE(RcSvStreamDecoder_MdRts);
   f9sv_TabSize         TabIdxBS_;
   f9sv_TabSize         TabIdxDeal_;
   f9sv_TabSize         TabIdxBase_;
   f9sv_TabSize         TabIdxRef_;

   const seed::Field*   FldBaseTDay_;
   const seed::Field*   FldBaseSession_;
   const seed::Field*   FldBaseSessionSt_;
   const seed::Field*   FldBaseMarket_;
   const seed::Field*   FldBaseFlowGroup_;
   const seed::Field*   FldBaseStrikePriceDiv_;
   const seed::Field*   FldBaseShUnit_;

   const seed::Field*   FldPriRef_;
   const seed::Field*   FldPriUpLmt_;
   const seed::Field*   FldPriDnLmt_;

   const seed::Field*   FldDealInfoTime_;
   const seed::Field*   FldDealTime_;
   const seed::Field*   FldDealTotalQty_;
   const seed::Field*   FldDealPri_;
   const seed::Field*   FldDealQty_;
   const seed::Field*   FldDealBuyCnt_;
   const seed::Field*   FldDealSellCnt_;
   const seed::Field*   FldDealFlags_;

   struct FieldPQ {
      const seed::Field*   FldPri_;
      const seed::Field*   FldQty_;
   };
   using FieldPQList = std::vector<FieldPQ>;
   FieldPQList          FldOrderBuys_;
   FieldPQList          FldOrderSells_;
   FieldPQList          FldDerivedBuys_;
   FieldPQList          FldDerivedSells_;
   const seed::Field*   FldBSInfoTime_;
   const seed::Field*   FldBSFlags_;

   RcSvStreamDecoder_MdRts(svc::TreeRec& tree) {
      // 配合 MdRts 的編碼, 從 tree 取出必要欄位(tab + field);
      auto* tab = GetTabOrRaise(*tree.Layout_, fon9_kCSTR_TabName_Deal);
      this->TabIdxDeal_      = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldDealTime_     = GetFieldOrRaise(*tab, "DealTime");
      this->FldDealPri_      = GetFieldOrRaise(*tab, "DealPri");
      // 指數行情, 只有 DealTime, DealPri;
      this->FldDealInfoTime_ = GetFieldOrNull(*tab, "InfoTime");
      this->FldDealTotalQty_ = GetFieldOrNull(*tab, "TotalQty");
      this->FldDealQty_      = GetFieldOrNull(*tab, "DealQty");
      this->FldDealBuyCnt_   = GetFieldOrNull(*tab, "DealBuyCnt");
      this->FldDealSellCnt_  = GetFieldOrNull(*tab, "DealSellCnt");
      this->FldDealFlags_    = GetFieldOrNull(*tab, "Flags");

      if ((tab = GetTabOrNull(*tree.Layout_, fon9_kCSTR_TabName_BS)) != nullptr) {
         this->TabIdxBS_ = static_cast<f9sv_TabSize>(tab->GetIndex());
         this->FldBSInfoTime_ = GetFieldOrRaise(*tab, "InfoTime");
         this->FldBSFlags_ = GetFieldOrRaise(*tab, "Flags");
         GetBSFields(this->FldOrderBuys_, tab->Fields_, 'B', '\0');
         GetBSFields(this->FldOrderSells_, tab->Fields_, 'S', '\0');
         GetBSFields(this->FldDerivedBuys_, tab->Fields_, 'D', 'B');
         GetBSFields(this->FldDerivedSells_, tab->Fields_, 'D', 'S');
      }

      tab = GetTabOrRaise(*tree.Layout_, fon9_kCSTR_TabName_Base);
      this->TabIdxBase_            = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldBaseTDay_           = GetFieldOrRaise(*tab, "TDay");
      this->FldBaseSession_        = GetFieldOrRaise(*tab, "Session");
      this->FldBaseSessionSt_      = GetFieldOrRaise(*tab, "SessionSt");
      this->FldBaseMarket_         = GetFieldOrRaise(*tab, "Market");
      this->FldBaseFlowGroup_      = GetFieldOrNull(*tab, "FlowGroup");
      this->FldBaseStrikePriceDiv_ = GetFieldOrNull(*tab, "StrikePriceDiv");
      this->FldBaseShUnit_         = GetFieldOrNull(*tab, "ShUnit");

      if ((tab = GetTabOrNull(*tree.Layout_, fon9_kCSTR_TabName_Ref)) != nullptr) {
         this->TabIdxRef_   = static_cast<f9sv_TabSize>(tab->GetIndex());
         this->FldPriRef_   = GetFieldOrRaise(*tab, "PriRef");
         this->FldPriUpLmt_ = GetFieldOrRaise(*tab, "PriUpLmt");
         this->FldPriDnLmt_ = GetFieldOrRaise(*tab, "PriDnLmt");
      }
   }
   static void GetBSFields(FieldPQList& dst, const seed::Fields& flds, char ch1, char ch2) {
      dst.reserve(fmkt::SymbBSData::kBSCount);
      NumOutBuf   nbuf;
      char* const pend = nbuf.end();
      // ch1 + ch2 + N + ('P' or 'Q'); e.g. "B1P", "B1Q"... "DB1P", "DB1Q";
      FieldPQ fld;
      for(unsigned L = 0;;) {
         char* pbeg = ToStrRev(pend - 1, ++L);
         if (ch2)
            *--pbeg = ch2;
         *--pbeg = ch1;
         StrView  fldName{pbeg, pend};
         *(pend - 1) = 'P';
         if ((fld.FldPri_ = flds.Get(fldName)) == nullptr)
            break;
         *(pend - 1) = 'Q';
         if ((fld.FldQty_ = flds.Get(fldName)) == nullptr)
            break;
         dst.push_back(fld);
      }
      dst.shrink_to_fit();
   }

   RcSvStreamDecoderNoteSP CreateDecoderNote() {
      return RcSvStreamDecoderNoteSP{new RcSvStreamDecoderNote_MdRts};
   }
   // -----
   void OnSubscribeStreamOK(svc::SubrRec& subr, StrView ack,
                            f9rc_ClientSession& ses, f9sv_ClientReport& rpt,
                            bool isNeedsLogResult) {
      assert(rpt.ResultCode_ == f9sv_Result_SubrStreamOK);
      // Pod snapshot, 打包函式:
      // - 訂閱單一商品: 使用 fon9::fmkt::SymbCellsToBitv() 打包;
      // - 訂閱整棵樹:   使用 MdSymbsBase::SubscribeStream() 打包;
      if (!ack.empty()) {
         DcQueueFixedMem dcq{ack};
         this->DecodeSnapshotSymb(subr, dcq, ses, rpt, isNeedsLogResult);
      }
      else if (auto fnOnHandler = subr.Seeds_[subr.TabIndex_]->Handler_.FnOnReport_) {
         // 僅訂閱歷史, 則沒有提供現在的 Pod snapshot.
         // 或訂閱整棵樹, 但沒有要求(或不提供)全部商品現在資料.
         fnOnHandler(&ses, &rpt);
      }
   }
   static void FetchSeedKey(svc::SubrRec& subr, DcQueue& dcq, CharVector& tempKeyText, f9sv_ClientReport& rpt) {
      BitvTo(dcq, tempKeyText);
      rpt.SeedKey_.Begin_ = tempKeyText.begin();
      rpt.SeedKey_.End_ = tempKeyText.end();
      rpt.SeedArray_ = svc::ToSeedArray(subr.Tree_->FetchPod(rpt.SeedKey_).Seeds_);
   }
   void DecodeSnapshotSymb(svc::SubrRec& subr, DcQueue& dcq,
                           f9rc_ClientSession& ses, f9sv_ClientReport& rpt,
                           bool isNeedsLogResult) {
      if (!rpt.SeedArray_)
         rpt.SeedArray_ = svc::ToSeedArray(subr.Seeds_);
      auto            fnOnHandler = subr.Seeds_[subr.TabIndex_]->Handler_.FnOnReport_;
      const bool      isSubrTree = seed::IsSubrTree(rpt.SeedKey_.Begin_);
      CharVector      tempKeyText; // 在 isSubrTree==true 時使用.
      const size_t    lastTabIdx = subr.Tree_->LayoutC_.TabCount_ - 1;
      size_t          tabidx = lastTabIdx;
      while (!dcq.empty()) {
         if (isSubrTree && tabidx == lastTabIdx)
            this->FetchSeedKey(subr, dcq, tempKeyText, rpt);
         BitvTo(dcq, tabidx);
         auto* tab = subr.Tree_->Layout_->GetTab(tabidx);
         assert(tab != nullptr);
         if (tab == nullptr) {
            fon9_LOG_ERROR("SnapshotSymb"
                           "|ses=", ToPtr(&ses),
                           "|tab=", tabidx,
                           "|err=Not found tab");
            return;
         }
         rpt.Tab_ = &subr.Tree_->LayoutC_.TabArray_[tabidx];
         rpt.Seed_ = rpt.SeedArray_[tabidx];
         seed::SimpleRawWr wr{static_cast<svc::SeedRec*>(const_cast<f9sv_Seed*>(rpt.Seed_))};
         size_t fldidx = 0;
         while (auto* fld = tab->Fields_.Get(fldidx++))
            fld->BitvToCell(wr, dcq);
         if (isNeedsLogResult)
            fon9_LOG_INFO("SnapshotSymb"
                          "|ses=", ToPtr(&ses),
                          "|tab=", tab->Name_);
         if (fnOnHandler)
            fnOnHandler(&ses, &rpt);
      }
   }
   void DecodeSnapshotSymbList(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      bool isNeedsLog = rx.IsNeedsLog_;
      rx.FlushLog();
      rpt.SeedKey_.Begin_ = fon9_kCSTR_SubrTree;
      rpt.SeedKey_.End_ = rpt.SeedKey_.Begin_ + sizeof(fon9_kCSTR_SubrTree) - 1;
      fon9_CStrView strSubrTree = rpt.SeedKey_;
      this->DecodeSnapshotSymb(*rx.SubrRec_, rxbuf, rx.Session_, rpt, isNeedsLog);
      rpt.SeedKey_ = strSubrTree;
   }
   void DecodeStreamRx(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      const fmkt::RtsPackType pkType = ReadOrRaise<fmkt::RtsPackType>(rxbuf);
      if (fon9_UNLIKELY(rx.IsNeedsLog_))
         RevPrint(rx.LogBuf_, "|pkType=", pkType);

      switch (pkType) {
      case fmkt::RtsPackType::DealPack:
      {
         DecoderAux  dec{rx, rpt, rxbuf, this->TabIdxDeal_};
         DecodeDealPack(dec, rx, rpt, rxbuf);
         assert(rxbuf.empty());
         return;
      }
      case fmkt::RtsPackType::SnapshotBS:
      case fmkt::RtsPackType::CalculatedBS:
      {
         DecoderAux dec(rx, rpt, rxbuf, this->TabIdxBS_);
         this->DecodeSnapshotBS(dec, rx, rpt, rxbuf,
            (pkType == fmkt::RtsPackType::CalculatedBS
             ? fmkt::BSFlag::Calculated : fmkt::BSFlag{}));
         return;
      }
      case fmkt::RtsPackType::UpdateBS:
         return this->DecodeUpdateBS(rx, rpt, rxbuf);
      case fmkt::RtsPackType::TradingSessionSt:
         return this->DecodeTradingSessionSt(rx, rpt, rxbuf);
      case fmkt::RtsPackType::BaseInfoTw:
         return this->DecodeBaseInfoTw(rx, rpt, rxbuf);
      case fmkt::RtsPackType::DealBS:
         return this->DecodeDealBS(rx, rpt, rxbuf);
      case fmkt::RtsPackType::SnapshotSymbList:
         return this->DecodeSnapshotSymbList(rx, rpt, rxbuf);
      case fmkt::RtsPackType::IndexValueV2:
         return this->DecodeIndexValueV2(rx, rpt, rxbuf);
      case fmkt::RtsPackType::IndexValueV2List:
         return this->DecodeIndexValueV2List(rx, rpt, rxbuf);
      case fmkt::RtsPackType::Count: // 增加此 case 僅是為了避免警告.
         break;
      }
   }
   void DecodeStreamData(svc::RxSubrData& rx, f9sv_ClientReport& rpt) {
      assert(rx.NotifyKind_ == seed::SeedNotifyKind::StreamData
             && rpt.ResultCode_ == f9sv_Result_NoError);
      DcQueueFixedMem rxbuf{rx.Gv_};
      this->DecodeStreamRx(rx, rpt, rxbuf);
   }
   void DecodeStreamRe(svc::RxSubrData& rxSrc, f9sv_ClientReport& rpt) {
      DcQueueFixedMem gvdcq{rxSrc.Gv_};
      size_t pksz;
      while (PopBitvByteArraySize(gvdcq, pksz)) {
         svc::RxSubrData rxRec{rxSrc};
         DcQueueFixedMem dcq{gvdcq.Peek1(), pksz};
         this->DecodeStreamRx(rxRec, rpt, dcq);
         gvdcq.PopConsumed(pksz);
      }
      assert(gvdcq.empty()); // 必定全部用完, 沒有剩餘資料.
   }
   void DecodeStreamRecover(svc::RxSubrData& rxSrc, f9sv_ClientReport& rpt) override {
      assert(rxSrc.NotifyKind_ == seed::SeedNotifyKind::StreamRecover
             && rpt.ResultCode_ == f9sv_Result_SubrStreamRecover);
      this->DecodeStreamRe(rxSrc, rpt);
      rxSrc.IsNeedsLog_ = false; // Log 已分配到 rxRec 去記錄, rxSrc 不用再記錄 Log.
   }
   void DecodeStreamRecoverEnd(svc::RxSubrData& rx, f9sv_ClientReport& rpt) override {
      assert(rx.NotifyKind_ == seed::SeedNotifyKind::StreamRecoverEnd
             && rpt.ResultCode_ == f9sv_Result_SubrStreamRecoverEnd);
      if (!rx.Gv_.empty()) {
         auto* tab = rpt.Tab_;
         rpt.ResultCode_ = f9sv_Result_SubrStreamRecover;
         this->DecodeStreamRe(rx, rpt);
         rpt.Tab_ = tab;
         rpt.Seed_ = nullptr;
         rpt.ResultCode_ = f9sv_Result_SubrStreamRecoverEnd;
      }
      this->NotifyStreamRpt(rx, rpt);
   }
   void DecodeStreamEnd(svc::RxSubrData& rx, f9sv_ClientReport& rpt) override {
      assert(rx.NotifyKind_ == seed::SeedNotifyKind::StreamEnd
             && rpt.ResultCode_ == f9sv_Result_SubrStreamEnd);
      this->NotifyStreamRpt(rx, rpt);
   }
   void NotifyStreamRpt(svc::RxSubrData& rx, f9sv_ClientReport& rpt) {
      rx.FlushLog();
      if (auto fnOnHandler = rx.SubrRec_->Seeds_[rx.SubrRec_->TabIndex_]->Handler_.FnOnReport_)
         fnOnHandler(&rx.Session_, &rpt);
   }
   // -----
   struct DecoderAux {
      fon9_NON_COPY_NON_MOVE(DecoderAux);
      RcSvStreamDecoderNote_MdRts*  Note_;
      DayTime*                      InfoTime_;
      svc::SeedRec*                 RptSeedRec_;
      seed::SimpleRawWr             RawWr_;

      DecoderAux(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf, f9sv_TabSize tabidx)
         : Note_{static_cast<RcSvStreamDecoderNote_MdRts*>(rx.SubrSeedRec_->StreamDecoderNote_.get())}
         , InfoTime_{Note_->SelectInfoTime(rx)}
         , RptSeedRec_{Note_->SetRptSeedArray(rpt, rx)[tabidx].get()}
         , RawWr_{*RptSeedRec_} {
         assert(dynamic_cast<RcSvStreamDecoderNote_MdRts*>(rx.SubrSeedRec_->StreamDecoderNote_.get()) != nullptr);
         rpt.Seed_ = this->RptSeedRec_;
         rpt.Tab_ = &rx.SubrRec_->Tree_->LayoutC_.TabArray_[tabidx];
         BitvToDayTimeOrUnchange(rxbuf, *this->InfoTime_);
      }
      DecoderAux(DecoderAux& src, f9sv_ClientReport& rpt, f9sv_TabSize tabidx)
         : Note_{src.Note_}
         , InfoTime_{src.InfoTime_}
         , RptSeedRec_{static_cast<svc::SeedRec*>(const_cast<f9sv_Seed*>(rpt.SeedArray_[tabidx]))}
         , RawWr_{*RptSeedRec_} {
         rpt.Seed_ = this->RptSeedRec_;
         rpt.Tab_ = &(rpt.Tab_ - rpt.Tab_->Named_.Index_)[tabidx];
      }
      template <class DecT>
      void PutDecField(const seed::Field& fld, DecT val) {
         fld.PutNumber(this->RawWr_, val.GetOrigValue(), val.Scale);
      }
   };
   // -----
   fmkt::DealFlag DecodeDealPack(DecoderAux& dec, svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DayTime  dealTime = *dec.InfoTime_;
      dec.PutDecField(*this->FldDealInfoTime_, dealTime);

      fmkt::DealFlag dealFlags = ReadOrRaise<fmkt::DealFlag>(rxbuf);
      if (IsEnumContains(dealFlags, fmkt::DealFlag::DealTimeChanged)) {
         BitvToDayTimeOrUnchange(rxbuf, dealTime);
         dec.PutDecField(*this->FldDealTime_, dealTime);
      }

      // 取得現在的 totalQty, 試算階段不提供 totalQty, 所以將 pTotalQty 設為 nullptr, 表示不更新.
      fmkt::Qty  totalQty = unsigned_cast(this->FldDealTotalQty_->GetNumber(dec.RawWr_, 0, 0));
      fmkt::Qty* pTotalQty = IsEnumContains(dealFlags, fmkt::DealFlag::Calculated) ? nullptr : &totalQty;
      if (IsEnumContains(dealFlags, fmkt::DealFlag::TotalQtyLost)) {
         BitvTo(rxbuf, totalQty);
         if (pTotalQty == nullptr)
            this->FldDealTotalQty_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(totalQty), 0);
      }

      auto     fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_;
      unsigned count = ReadOrRaise<byte>(rxbuf);
      if (rx.IsNeedsLog_) {
         RevPrint(rx.LogBuf_, "|rtDeal=", ToHex(dealFlags), "|count=", count + 1);
         rx.FlushLog();
      }
      if (count > 0) {
         this->FldDealFlags_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(
            dealFlags - (fmkt::DealFlag::DealBuyCntChanged | fmkt::DealFlag::DealSellCntChanged)), 0);
         this->DecodeDealPQ(rxbuf, dec.RawWr_, pTotalQty);
         if (fnOnReport)
            fnOnReport(&rx.Session_, &rpt);
         dealFlags -= (fmkt::DealFlag::TotalQtyLost | fmkt::DealFlag::DealTimeChanged);
         if (--count > 0) {
            this->FldDealFlags_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(
               dealFlags - (fmkt::DealFlag::DealBuyCntChanged | fmkt::DealFlag::DealSellCntChanged)), 0);
            do {
               this->DecodeDealPQ(rxbuf, dec.RawWr_, pTotalQty);
               if (fnOnReport)
                  fnOnReport(&rx.Session_, &rpt);
            } while (--count > 0);
         }
      }
      this->FldDealFlags_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(dealFlags), 0);
      this->DecodeDealPQ(rxbuf, dec.RawWr_, pTotalQty);
      this->DecodeDealCnt(rxbuf, dec.RawWr_, dealFlags);
      if (fnOnReport)
         fnOnReport(&rx.Session_, &rpt);
      return dealFlags;
   }
   void DecodeDealPQ(DcQueue& rxbuf, seed::SimpleRawWr& dealwr, fmkt::Qty* pTotalQty) {
      this->FldDealPri_->BitvToCell(dealwr, rxbuf);
      fmkt::Qty dealQty{0};
      BitvTo(rxbuf, dealQty);
      this->FldDealQty_->PutNumber(dealwr, static_cast<seed::FieldNumberT>(dealQty), 0);
      if (pTotalQty) {
         *pTotalQty += dealQty;
         this->FldDealTotalQty_->PutNumber(dealwr, static_cast<seed::FieldNumberT>(*pTotalQty), 0);
      }
   }
   void DecodeDealCnt(DcQueue& rxbuf, seed::SimpleRawWr& dealwr, const fmkt::DealFlag dealFlags) {
      if (IsEnumContains(dealFlags, fmkt::DealFlag::DealBuyCntChanged))
         this->FldDealBuyCnt_->BitvToCell(dealwr, rxbuf);
      if (IsEnumContains(dealFlags, fmkt::DealFlag::DealSellCntChanged))
         this->FldDealSellCnt_->BitvToCell(dealwr, rxbuf);
   }
   void DecodeDealBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux     decDeal{rx, rpt, rxbuf, this->TabIdxDeal_};
      fmkt::DealFlag dflag = this->DecodeDealPack(decDeal, rx, rpt, rxbuf);
      DecoderAux     decBS{decDeal, rpt, this->TabIdxBS_};
      this->DecodeSnapshotBS(decBS, rx, rpt, rxbuf,
         (IsEnumContains(dflag, fmkt::DealFlag::Calculated)
          ? fmkt::BSFlag::Calculated : fmkt::BSFlag{}));
   }
   // -----
   static void ClearFieldValues(const seed::RawWr& wr, FieldPQList::const_iterator ibeg, FieldPQList::const_iterator iend) {
      for (; ibeg != iend; ++ibeg) {
         ibeg->FldPri_->SetNull(wr);
         ibeg->FldQty_->SetNull(wr);
      }
   }
   static void RevPrintPQ(RevBuffer& rbuf, const FieldPQ& fld, const seed::RawRd& rd) {
      fld.FldQty_->CellRevPrint(rd, nullptr, rbuf);
      RevPrint(rbuf, '*');
      fld.FldPri_->CellRevPrint(rd, nullptr, rbuf);
   }
   void DecodeSnapshotBS(DecoderAux& dec, svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf, fmkt::BSFlag bsFlags) {
      dec.PutDecField(*this->FldBSInfoTime_, *dec.InfoTime_);
      const FieldPQList* flds;
      while (!rxbuf.empty()) {
         const uint8_t first = ReadOrRaise<uint8_t>(rxbuf);
         switch (static_cast<fmkt::RtBSType>(first & cast_to_underlying(fmkt::RtBSType::Mask))) {
         #define case_RtBSType(type) case fmkt::RtBSType::type: \
            flds = &this->Fld##type##s_; \
            bsFlags |= fmkt::BSFlag::type; \
            break
         // -----
         case_RtBSType(OrderBuy);
         case_RtBSType(OrderSell);
         case_RtBSType(DerivedBuy);
         case_RtBSType(DerivedSell);
         default:
            assert(!"Unknown RtBSType.");
            return;
         }
         assert(!flds->empty());
         unsigned count = static_cast<unsigned>(first & 0x0f) + 1;
         auto     ibeg = flds->cbegin() + count;
         this->ClearFieldValues(dec.RawWr_, ibeg, flds->cend());
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, '}');
         do {
            --ibeg;
            ibeg->FldPri_->BitvToCell(dec.RawWr_, rxbuf);
            ibeg->FldQty_->BitvToCell(dec.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_)) {
               RevPrintPQ(rx.LogBuf_, *ibeg, dec.RawWr_);
               if (count > 1)
                  RevPrint(rx.LogBuf_, '|');
            }
         } while (--count > 0);
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, "|rtBS.", ToHex(first), "={");
      }
      // -----
      #define check_ClearFieldValues(type) \
      while (!IsEnumContains(bsFlags, fmkt::BSFlag::type)) { \
         this->ClearFieldValues(dec.RawWr_, this->Fld##type##s_.cbegin(), this->Fld##type##s_.cend()); \
         break; \
      }
      // -----
      check_ClearFieldValues(OrderBuy);
      check_ClearFieldValues(OrderSell);
      check_ClearFieldValues(DerivedBuy);
      check_ClearFieldValues(DerivedSell);
      // -----
      this->ReportBS(rx, rpt, dec.RawWr_, bsFlags);
   }
   void ReportBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, const seed::RawWr& wr, fmkt::BSFlag bsFlags) {
      this->FldBSFlags_->PutNumber(wr, static_cast<seed::FieldNumberT>(bsFlags), 0);
      this->ReportEv(rx, rpt);
   }
   static void ReportEv(svc::RxSubrData& rx, f9sv_ClientReport& rpt) {
      rx.FlushLog();
      if (auto fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_)
         fnOnReport(&rx.Session_, &rpt);
   }
   // -----
   static void CopyPQ(const seed::RawWr& wr, const FieldPQ& dst, const FieldPQ& src) {
      const auto srcPriNull = src.FldPri_->GetNullValue();
      const auto srcPriScale = src.FldPri_->DecScale_;
      dst.FldPri_->PutNumber(wr, src.FldPri_->GetNumber(wr, srcPriScale, srcPriNull), srcPriScale);
      dst.FldQty_->PutNumber(wr, src.FldQty_->GetNumber(wr, 0, 0), 0);
   }
   static void DeletePQ(const seed::RawWr& wr, FieldPQList::const_iterator ibeg, FieldPQList::const_iterator iend) {
      assert(ibeg != iend);
      const FieldPQ* prev = &*ibeg;
      // 刪除 ibeg: ibeg 之後往前移動; 清除 iend-1;
      while (++ibeg != iend) {
         const FieldPQ* curr = &*ibeg;
         CopyPQ(wr, *prev, *curr);
         prev = curr;
      }
      prev->FldPri_->SetNull(wr);
      prev->FldQty_->SetNull(wr);
   }
   static void InsertPQ(const seed::RawWr& wr, FieldPQList::const_iterator ibeg, FieldPQList::const_iterator iend) {
      while (--iend != ibeg)
         CopyPQ(wr, *iend, *(iend - 1));
   }
   void DecodeUpdateBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux dec(rx, rpt, rxbuf, this->TabIdxBS_);
      dec.PutDecField(*this->FldBSInfoTime_, *dec.InfoTime_);
      const uint8_t first = ReadOrRaise<uint8_t>(rxbuf);
      fmkt::BSFlag  bsFlags{};
      if (first & 0x80)
         bsFlags |= fmkt::BSFlag::Calculated;
      unsigned           count = (first & 0x7fu);
      const FieldPQList* flds;
      for (;;) {
         const uint8_t bsType = ReadOrRaise<uint8_t>(rxbuf);
         switch (static_cast<fmkt::RtBSType>(bsType & cast_to_underlying(fmkt::RtBSType::Mask))) {
         case_RtBSType(OrderBuy);
         case_RtBSType(OrderSell);
         case_RtBSType(DerivedBuy);
         case_RtBSType(DerivedSell);
         default:
            assert(!"Unknown RtBSType.");
            return;
         }
         assert((bsType & 0x0fu) < flds->size());
         if ((bsType & 0x0fu) >= flds->size()) {
            fon9_LOG_ERROR("DecodeUpdateBS|err=Bad level|rtBS=", ToHex(bsType));
            return;
         }
         const auto ibeg = flds->cbegin() + (bsType & 0x0f);
         switch (static_cast<fmkt::RtBSAction>(bsType & cast_to_underlying(fmkt::RtBSAction::Mask))) {
         case fmkt::RtBSAction::New:
            InsertPQ(dec.RawWr_, ibeg, flds->cend());
            // 不用 break; 取出 rxbuf 裡面的 Pri,Qty; 填入 ibeg;
         case fmkt::RtBSAction::ChangePQ:
            ibeg->FldPri_->BitvToCell(dec.RawWr_, rxbuf);
            ibeg->FldQty_->BitvToCell(dec.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_))
               RevPrintPQ(rx.LogBuf_, *ibeg, dec.RawWr_);
            break;
         case fmkt::RtBSAction::ChangeQty:
            ibeg->FldQty_->BitvToCell(dec.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_))
               ibeg->FldQty_->CellRevPrint(dec.RawWr_, nullptr, rx.LogBuf_);
            break;
         case fmkt::RtBSAction::Delete:
            DeletePQ(dec.RawWr_, ibeg, flds->cend());
            break;
         default:
            assert(!"Unknown RtBSAction.");
            fon9_LOG_ERROR("DecodeUpdateBS|err=Unknown RtBSAction|rtBS=", ToHex(bsType));
            return;
         }
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, "|rtBS.", ToHex(bsType), '=');
         if (count <= 0)
            break;
         --count;
      }
      assert(count == 0 && rxbuf.empty());
      // -----
      this->ReportBS(rx, rpt, dec.RawWr_, bsFlags);
   }
   // -----
   static void BitvToIndexValue(const seed::Field& fld, seed::RawWr& wr, DcQueue& rxbuf) {
      fon9_BitvNumR numr;
      BitvToNumber(rxbuf, numr);
      if (numr.Type_ == fon9_BitvNumT_Null)
         fld.SetNull(wr);
      else
         fld.PutNumber(wr, signed_cast(numr.Num_), static_cast<DecScaleT>(numr.Scale_ + 2));
   }
   void DecodeIndexValueV2(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux  dec{rx, rpt, rxbuf, this->TabIdxDeal_};
      DayTime     dealTime = *dec.InfoTime_;
      dec.PutDecField(*this->FldDealTime_, dealTime);
      BitvToIndexValue(*this->FldDealPri_, dec.RawWr_, rxbuf);
      if (rx.IsNeedsLog_) {
         this->FldDealPri_->CellRevPrint(dec.RawWr_, nullptr, rx.LogBuf_);
         RevPrint(rx.LogBuf_, "|infoTime=", dealTime, "|value=");
         rx.FlushLog();
      }
      if (auto fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_)
         fnOnReport(&rx.Session_, &rpt);
      assert(rxbuf.empty());
   }
   void DecodeIndexValueV2List(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      auto        fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_;
      DecoderAux  dec{rx, rpt, rxbuf, this->TabIdxDeal_};
      DayTime     dealTime = *dec.InfoTime_;
      dec.PutDecField(*this->FldDealTime_, dealTime);
      CharVector  tempKeyText;
      while (!rxbuf.empty()) {
         this->FetchSeedKey(*rx.SubrRec_, rxbuf, tempKeyText, rpt);
         rpt.Tab_ = &rx.SubrRec_->Tree_->LayoutC_.TabArray_[this->TabIdxDeal_];
         rpt.Seed_ = rpt.SeedArray_[this->TabIdxDeal_];
         seed::SimpleRawWr wr{static_cast<svc::SeedRec*>(const_cast<f9sv_Seed*>(rpt.Seed_))};
         this->FldDealTime_->PutNumber(wr, dealTime.GetOrigValue(), dealTime.Scale);
         BitvToIndexValue(*this->FldDealPri_, wr, rxbuf);
         if (rx.IsNeedsLog_) {
            this->FldDealPri_->CellRevPrint(wr, nullptr, rx.LogBuf_);
            RevPrint(rx.LogBuf_, '|', tempKeyText, '=');
         }
         if (fnOnReport)
            fnOnReport(&rx.Session_, &rpt);
      }
      if (rx.IsNeedsLog_)
         RevPrint(rx.LogBuf_, "|infoTime=", dealTime);
   }
   // -----
   void DecodeTradingSessionSt(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux  dec(rx, rpt, rxbuf, this->TabIdxBase_);
      uint32_t    tdayYYYYMMDD = ReadOrRaise<uint32_t>(rxbuf);
      char        sesId = cast_to_underlying(ReadOrRaise<f9fmkt_TradingSessionId>(rxbuf));
      auto        sesSt = ReadOrRaise<f9fmkt_TradingSessionSt>(rxbuf);
      this->FldBaseTDay_->PutNumber(dec.RawWr_, tdayYYYYMMDD, 0);
      this->FldBaseSession_->StrToCell(dec.RawWr_, StrView{&sesId, 1});
      this->FldBaseSessionSt_->PutNumber(dec.RawWr_, sesSt, 0);
      assert(rxbuf.empty());
      this->ReportEv(rx, rpt);
   }
   void DecodeBaseInfoTw(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux  decBase(rx, rpt, rxbuf, this->TabIdxBase_);

      char  chTradingMarket = cast_to_underlying(ReadOrRaise<f9fmkt_TradingMarket>(rxbuf));
      this->FldBaseMarket_->StrToCell(decBase.RawWr_, StrView{&chTradingMarket, 1});

      if (this->FldBaseFlowGroup_) {
         uint8_t  u8FlowGroup = ReadOrRaise<uint8_t>(rxbuf);
         this->FldBaseFlowGroup_->PutNumber(decBase.RawWr_, u8FlowGroup, 0);
      }
      if (this->FldBaseStrikePriceDiv_) {
         uint8_t  u8StrikePriceDecimalLocator = ReadOrRaise<uint8_t>(rxbuf);
         this->FldBaseStrikePriceDiv_->PutNumber(decBase.RawWr_, static_cast<seed::FieldNumberT>(fon9::GetDecDivisor(u8StrikePriceDecimalLocator)), 0);
      }
      if (this->FldBaseShUnit_) {
         this->FldBaseShUnit_->BitvToCell(decBase.RawWr_, rxbuf);
      }
      this->ReportEv(rx, rpt);

      DecoderAux  decRef(decBase, rpt, this->TabIdxRef_);
      this->FldPriRef_->BitvToCell(decRef.RawWr_, rxbuf);
      this->FldPriUpLmt_->BitvToCell(decRef.RawWr_, rxbuf);
      this->FldPriDnLmt_->BitvToCell(decRef.RawWr_, rxbuf);
      assert(rxbuf.empty());
      this->ReportEv(rx, rpt);
   }
};

} } // namespaces
//--------------------------------------------------------------------------//
extern "C" {

f9sv_CAPI_FN(f9sv_Result) f9sv_AddStreamDecoder_MdRts(void) {
   using namespace fon9::rc;
   struct Factory : public RcSvStreamDecoderFactory {
      Factory() {
         RcSvStreamDecoderPark::Register("MdRts", this);
      }
      RcSvStreamDecoderSP CreateStreamDecoder(svc::TreeRec& tree) override {
         return RcSvStreamDecoderSP{new RcSvStreamDecoder_MdRts{tree}};
      }
   };
   static Factory factory;
   return f9sv_Result_NoError;
}

} // extern "C"
