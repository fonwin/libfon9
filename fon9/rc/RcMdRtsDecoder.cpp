// \file fon9/rc/RcMdRtsDecoder.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcMdRtsDecoder.hpp"
#include "fon9/fmkt/SymbDealData.hpp"
#include "fon9/fmkt/SymbBSData.hpp"
#include "fon9/fmkt/SymbTabNames.h"
#include "fon9/seed/RawWr.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 { namespace rc {

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

RcMdRtsDecoder_TabFields::RcMdRtsDecoder_TabFields(svc::TreeRec& tree) {
   this->RcMdRtsDecoder_TabFields_POD_ctor();
   // 配合 MdRts 的編碼, 從 tree 取出必要欄位(tab + field);
   auto* tab = GetTabOrRaise(*tree.Layout_, fon9_kCSTR_TabName_Base);
   this->TabIdxBase_            = static_cast<f9sv_TabSize>(tab->GetIndex());
   this->FldBaseTDay_           = GetFieldOrRaise(*tab, "TDay");
   this->FldBaseSession_        = GetFieldOrRaise(*tab, "Session");
   this->FldBaseSessionSt_      = GetFieldOrRaise(*tab, "SessionSt");
   this->FldBaseMarket_         = GetFieldOrRaise(*tab, "Market");
   this->FldBaseFlowGroup_      = GetFieldOrNull (*tab, "FlowGroup");
   this->FldBasePriceOrigDiv_   = GetFieldOrNull (*tab, "PriceOrigDiv");
   this->FldBaseStrikePriceDiv_ = GetFieldOrNull (*tab, "StrikePriceDiv");
   this->FldBaseShUnit_         = GetFieldOrNull (*tab, "ShUnit");
   this->FldBaseTwsFlags_       = GetFieldOrNull (*tab, "TwsFlags");

   // 期貨契約資料表, 沒有 Deal tab.
   if ((tab = GetTabOrNull(*tree.Layout_, fon9_kCSTR_TabName_Deal)) != nullptr) {
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
      this->FldDealLmtFlags_ = GetFieldOrNull(*tab, "LmtFlags");
      this->FldIdxDealMktSeq_= GetFieldOrNull(*tab, "MktSeq");
   }
   if ((tab = GetTabOrNull(*tree.Layout_, fon9_kCSTR_TabName_BS)) != nullptr) {
      this->TabIdxBS_      = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldBSInfoTime_ = GetFieldOrRaise(*tab, "InfoTime");
      this->FldBSFlags_    = GetFieldOrRaise(*tab, "Flags");
      this->FldBSLmtFlags_ = GetFieldOrNull (*tab, "LmtFlags");
      this->FldBSMktSeq_   = GetFieldOrNull (*tab, "MktSeq");
      SetBSFields(this->FldOrderBuys_,    tab->Fields_, 'B', '\0');
      SetBSFields(this->FldOrderSells_,   tab->Fields_, 'S', '\0');
      SetBSFields(this->FldDerivedBuys_,  tab->Fields_, 'D', 'B');
      SetBSFields(this->FldDerivedSells_, tab->Fields_, 'D', 'S');
   }
   if ((tab = GetTabOrNull(*tree.Layout_, fon9_kCSTR_TabName_Ref)) != nullptr) {
      static const char kCSTR_PriUpLmt[] = "PriUpLmt";
      static const char kCSTR_PriDnLmt[] = "PriDnLmt";
      this->TabIdxRef_   = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldPriRef_   = GetFieldOrRaise(*tab, "PriRef");
      this->FldPriUpLmt_ = GetFieldOrRaise(*tab, kCSTR_PriUpLmt);
      this->FldPriDnLmt_ = GetFieldOrRaise(*tab, kCSTR_PriDnLmt);
      this->FldLvUpLmt_  = GetFieldOrNull (*tab, "LvUpLmt");
      this->FldLvDnLmt_  = GetFieldOrNull (*tab, "LvDnLmt");
      NumOutBuf   nbuf;
      char* const pend = nbuf.end();
      FieldPriLmt fldLmt;
      for (unsigned lvLmt = 0;;) {
         char* pbeg = ToStrRev(pend, ++lvLmt);
         memcpy(pbeg -= sizeof(kCSTR_PriUpLmt) - 1, kCSTR_PriUpLmt, sizeof(kCSTR_PriUpLmt) - 1);
         if ((fldLmt.Up_ = GetFieldOrNull(*tab, StrView(pbeg, pend))) != nullptr) {
            memcpy(pbeg, kCSTR_PriDnLmt, sizeof(kCSTR_PriDnLmt) - 1);
            if ((fldLmt.Dn_ = GetFieldOrNull(*tab, StrView(pbeg, pend))) != nullptr) {
               this->FldPriLmts_.push_back(fldLmt);
               continue;
            }
         }
         break;
      }
   }
}
void RcMdRtsDecoder_TabFields::SetBSFields(FieldPQList& dst, const seed::Fields& flds, char ch1, char ch2) {
   dst.reserve(fmkt::SymbBSData::kBSCount);
   NumOutBuf   nbuf;
   char* const pend = nbuf.end();
   // ch1 + ch2 + N + ('P' or 'Q'); e.g. "B1P", "B1Q"... "DB1P", "DB1Q";
   FieldPQ fld;
   for (unsigned L = 0;;) {
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
//--------------------------------------------------------------------------//
/// 如果為 Null 表示: 不變動 dst;
static void BitvToDayTimeOrUnchange(DcQueue& rxbuf, DayTime& dst) {
   DayTime val{DayTime::Null()};
   BitvTo(rxbuf, val);
   if (!val.IsNull())
      dst = val;
}
//--------------------------------------------------------------------------//
class RcSvStreamDecoderNote_MdRts : public RcMdRtsDecoderNote {
   fon9_NON_COPY_NON_MOVE(RcSvStreamDecoderNote_MdRts);
   DayTime              RtInfoTime_{DayTime::Null()};
   DayTime              ReInfoTime_{DayTime::Null()};
   svc::PodRec          RePod_;

public:
   RcSvStreamDecoderNote_MdRts() = default;
   // 根據 rx.NotifyKind_ 選擇「回補 or 及時」使用的 InfoTime.
   DayTime* SelectInfoTime(svc::RxSubrData& rx) {
      return (rx.NotifyKind_ == seed::SeedNotifyKind::StreamData
              ? &this->RtInfoTime_ : &this->ReInfoTime_);
   }
   // 根據 rx.NotifyKind_ 選擇「回補 or 及時」使用的 SeedArray.
   const f9sv_Seed** SelectSeedArray(svc::RxSubrData& rx) {
      if (rx.NotifyKind_ == seed::SeedNotifyKind::StreamData) {
         if (!rx.IsSubrTree_ || rx.SeedKey_.empty() || seed::IsSubrTree(rx.SeedKey_.begin()))
            return ToSeedArray(rx.SubrRec_->Seeds_);
         return ToSeedArray(rx.SubrRec_->Tree_->FetchPod(ToStrView(rx.SeedKey_)).Seeds_);
      }
      if (this->RePod_.Seeds_ == nullptr)
         rx.SubrRec_->Tree_->MakePod(this->RePod_);
      return ToSeedArray(this->RePod_.Seeds_);
   }

   bool IsRxMktSeqNewer(const seed::Field& fld, const seed::RawWr& wr, svc::RxSubrData& rx, DcQueue& rxbuf) {
      uint64_t rxSeq{};
      BitvTo(rxbuf, rxSeq);
      if (fon9_UNLIKELY(rx.IsNeedsLog_))
         RevPrint(rx.LogBuf_, "MktSeq=", rxSeq);

      if (fon9_LIKELY(rx.NotifyKind_ == seed::SeedNotifyKind::StreamData)) {
         if (!this->IsMktSeqNewer(fld, wr, rxSeq)) {
            if (fon9_UNLIKELY(rx.IsNeedsLog_))
               RevPrint(rx.LogBuf_, "|Old.");
            return false;
         }
      }
      else {
         // rx.NotifyKind_ == Recover; 不用考慮 mktseq 是否為過期資料.
         fld.PutNumber(wr, signed_cast(rxSeq), 0);
      }
      if (fon9_UNLIKELY(rx.IsNeedsLog_))
         RevPrint(rx.LogBuf_, '|');
      return true;
   }
};

struct MdRtsDecoderAuxNote {
   fon9_NON_COPY_NON_MOVE(MdRtsDecoderAuxNote);
   RcSvStreamDecoderNote_MdRts&  Note_;

   MdRtsDecoderAuxNote(svc::RxSubrData& rx, f9sv_ClientReport& rpt)
      : Note_(*static_cast<RcSvStreamDecoderNote_MdRts*>(rx.SubrSeedRec_->StreamDecoderNote_.get())) {
      assert(dynamic_cast<RcSvStreamDecoderNote_MdRts*>(rx.SubrSeedRec_->StreamDecoderNote_.get()) != nullptr);
      rpt.SeedArray_ = this->Note_.SelectSeedArray(rx);
   }
   MdRtsDecoderAuxNote(MdRtsDecoderAuxNote& src, f9sv_ClientReport& rpt, f9sv_TabSize tabidx)
      : Note_(src.Note_) {
      SetRptTabSeed(rpt, tabidx);
   }
};
using NoteAux = MdRtsDecoderAuxNote;

struct MdRtsDecoderAuxInfo : public MdRtsDecoderAuxNote {
   fon9_NON_COPY_NON_MOVE(MdRtsDecoderAuxInfo);
   DayTime* const    SeInfoTime_;
   seed::SimpleRawWr RawWr_;

   MdRtsDecoderAuxInfo(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf, f9sv_TabSize tabidx)
      : MdRtsDecoderAuxNote{rx, rpt}
      , SeInfoTime_{Note_.SelectInfoTime(rx)}
      , RawWr_{*SetRptTabSeed(rpt, tabidx)} {
      BitvToDayTimeOrUnchange(rxbuf, *this->SeInfoTime_);
   }
   MdRtsDecoderAuxInfo(MdRtsDecoderAuxInfo& src, f9sv_ClientReport& rpt, f9sv_TabSize tabidx)
      : MdRtsDecoderAuxNote{src, rpt, tabidx}
      , SeInfoTime_{src.SeInfoTime_}
      , RawWr_{const_cast<f9sv_Seed*>(rpt.Seed_)} {
   }
   template <class DecT>
   void PutDecField(const seed::Field& fld, DecT val) {
      fld.PutNumber(this->RawWr_, val.GetOrigValue(), val.Scale);
   }
};
using InfoAux = MdRtsDecoderAuxInfo;

struct RcSvStreamDecoder_MdRts : public RcMdRtsDecoder {
   fon9_NON_COPY_NON_MOVE(RcSvStreamDecoder_MdRts);

   using RcMdRtsDecoder::RcMdRtsDecoder;

   RcSvStreamDecoderNoteSP CreateDecoderNote() override {
      return RcSvStreamDecoderNoteSP{new RcSvStreamDecoderNote_MdRts};
   }
   // -----
   void OnSubscribeStreamOK(svc::SubrRec& subr, StrView ack,
                            f9rc_ClientSession& ses, f9sv_ClientReport& rpt,
                            bool isNeedsLogResult) override {
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
      rpt.SeedKey_.End_   = tempKeyText.end();
      rpt.SeedArray_      = svc::ToSeedArray(subr.Tree_->FetchPod(rpt.SeedKey_).Seeds_);
   }
   void DecodeSnapshotSymb(svc::SubrRec& subr, DcQueue& dcq,
                           f9rc_ClientSession& ses, f9sv_ClientReport& rpt,
                           bool isNeedsLogResult) {
      if (!rpt.SeedArray_)
         rpt.SeedArray_ = svc::ToSeedArray(subr.Seeds_);
      const auto     fnOnHandler = subr.Seeds_[subr.TabIndex_]->Handler_.FnOnReport_;
      const bool     isSubrTree = seed::IsSubrTree(rpt.SeedKey_.Begin_);
      CharVector     tempKeyText; // 在 isSubrTree==true 時使用.
      const auto     tabCount = rpt.Layout_->TabCount_;
      f9sv_TabSize   tabidx = 0;
      while (!dcq.empty()) {
         if (isSubrTree && tabidx == 0)
            this->FetchSeedKey(subr, dcq, tempKeyText, rpt);
         auto* tab = subr.Tree_->Layout_->GetTab(tabidx);
         assert(tab != nullptr);
         if (tab == nullptr) {
            fon9_LOG_ERROR("SnapshotSymb"
                           "|ses=", ToPtr(&ses),
                           "|tab=", tabidx,
                           "|err=Not found tab");
            return;
         }
         seed::SimpleRawWr wr{SetRptTabSeed(rpt, tabidx)};
         size_t fldidx = 0;
         while (auto* fld = tab->Fields_.Get(fldidx++))
            fld->BitvToCell(wr, dcq);
         if (isNeedsLogResult)
            fon9_LOG_INFO("SnapshotSymb"
                          "|ses=", ToPtr(&ses),
                          "|tab=", tab->Name_);
         if (fnOnHandler)
            fnOnHandler(&ses, &rpt);
         if (this->FldBSMktSeq_) {
            const auto* seedRt = static_cast<const svc::SeedRec*>(rpt.Seed_);
            if (auto* note = static_cast<RcMdRtsDecoderNote*>(seedRt->StreamDecoderNote_.get())) {
               assert(dynamic_cast<RcMdRtsDecoderNote*>(seedRt->StreamDecoderNote_.get()));
               note->ResetMktSeq(unsigned_cast(this->FldBSMktSeq_->GetNumber(seed::SimpleRawRd{rpt.SeedArray_[this->TabIdxBS_]}, 0, 0)));
               if (this->FldBasePriceOrigDiv_) {
                  auto dv = this->FldBasePriceOrigDiv_->GetNumber(seed::SimpleRawRd{rpt.SeedArray_[this->TabIdxBase_]}, 0, 0);
                  DecScaleT scale = this->FldPriRef_->DecScale_;
                  while ((dv /= 10) > 0)
                     --scale;
                  if (scale <= this->FldPriRef_->DecScale_)
                     note->SetPriScale(scale);
               }
            }
         }
         if (++tabidx >= tabCount)
            tabidx = 0;
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
      const f9sv_RtsPackType pkType = ReadOrRaise<f9sv_RtsPackType>(rxbuf);
      rpt.StreamPackType_ = cast_to_underlying(pkType);
      if (fon9_UNLIKELY(rx.IsNeedsLog_))
         RevPrint(rx.LogBuf_, "|pkType=", pkType);
      switch (pkType) {
      case f9sv_RtsPackType_DealPack:
         this->DecodeDealPack(InfoAux{rx, rpt, rxbuf, this->TabIdxDeal_}, rx, rpt, rxbuf);
         return;
      case f9sv_RtsPackType_CalculatedBS:
      case f9sv_RtsPackType_SnapshotBS:
      {
         InfoAux  aux{rx, rpt, rxbuf, this->TabIdxBS_};
         if (this->IsBSMktSeqNewer(aux, rx, rxbuf))
            this->DecodeSnapshotBS(std::move(aux), rx, rpt, rxbuf,
                                   pkType == f9sv_RtsPackType_CalculatedBS ? f9sv_BSFlag_Calculated : f9sv_BSFlag{});
         return;
      }
      case f9sv_RtsPackType_UpdateBS:
         return this->DecodeUpdateBS(rx, rpt, rxbuf);
      case f9sv_RtsPackType_DealBS:
         return this->DecodeDealBS(rx, rpt, rxbuf);

      case f9sv_RtsPackType_TradingSessionId:
         return this->DecodeTradingSessionId(rx, rpt, rxbuf);
      case f9sv_RtsPackType_BaseInfoTw:
         return this->DecodeBaseInfoTw(rx, rpt, rxbuf);
      case f9sv_RtsPackType_PriLmts:
         return this->DecodePriLmts(InfoAux{rx, rpt, rxbuf, this->TabIdxRef_}, rx, rpt, rxbuf);
      case f9sv_RtsPackType_SnapshotSymbList_NoInfoTime:
         return this->DecodeSnapshotSymbList(rx, rpt, rxbuf);
      case f9sv_RtsPackType_IndexValueV2:
         return this->DecodeIndexValueV2(rx, rpt, rxbuf);
      case f9sv_RtsPackType_IndexValueV2List:
         return this->DecodeIndexValueV2List(rx, rpt, rxbuf);
      case f9sv_RtsPackType_FieldValue_NoInfoTime:
         return this->DecodeFieldValue_NoInfoTime(rx, rpt, rxbuf);
      case f9sv_RtsPackType_FieldValue_AndInfoTime:
         return this->DecodeFieldValue_AndInfoTime(rx, rpt, rxbuf);
      case f9sv_RtsPackType_TabValues_NoInfoTime:
         return this->DecodeTabValues_NoInfoTime(rx, rpt, rxbuf);
      case f9sv_RtsPackType_TabValues_AndInfoTime:
         return this->DecodeTabValues_AndInfoTime(rx, rpt, rxbuf);
      case f9sv_RtsPackType_Count: // 增加此 case 僅是為了避免警告.
         break;
      }
   }
   void DecodeStreamData(svc::RxSubrData& rx, f9sv_ClientReport& rpt) override {
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
      if (auto fnOnHandler = rx.SubrSeedRec_->Handler_.FnOnReport_)
         fnOnHandler(&rx.Session_, &rpt);
   }
   // -----
   #define f9sv_DealFlag_IsOld   static_cast<f9sv_DealFlag>(0xff)
   f9sv_DealFlag DecodeDealPack(InfoAux&& aux, svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      if (this->FldBSMktSeq_) {
         if (!aux.Note_.IsRxMktSeqNewer(*this->FldBSMktSeq_, seed::SimpleRawWr{const_cast<f9sv_Seed*>(rpt.SeedArray_[this->TabIdxBS_])},
                                        rx, rxbuf))
            return f9sv_DealFlag_IsOld;
      }

      DayTime  dealTime = *aux.SeInfoTime_;
      aux.PutDecField(*this->FldDealInfoTime_, dealTime);

      f9sv_DealFlag  dealFlags = ReadOrRaise<f9sv_DealFlag>(rxbuf);
      if (IsEnumContains(dealFlags, f9sv_DealFlag_DealTimeChanged)) {
         BitvToDayTimeOrUnchange(rxbuf, dealTime);
         aux.PutDecField(*this->FldDealTime_, dealTime);
      }

      // 取得現在的 totalQty, 試算階段不提供 totalQty, 所以將 pTotalQty 設為 nullptr, 表示不更新.
      fmkt::Qty  totalQty = unsigned_cast(this->FldDealTotalQty_->GetNumber(aux.RawWr_, 0, 0));
      fmkt::Qty* pTotalQty = IsEnumContains(dealFlags, f9sv_DealFlag_Calculated) ? nullptr : &totalQty;
      if (IsEnumContains(dealFlags, f9sv_DealFlag_TotalQtyLost)) {
         BitvTo(rxbuf, totalQty);
         if (pTotalQty == nullptr)
            this->FldDealTotalQty_->PutNumber(aux.RawWr_, static_cast<seed::FieldNumberT>(totalQty), 0);
      }
      if (IsEnumContains(dealFlags, f9sv_DealFlag_LmtFlagsChanged)) {
         assert(this->FldDealLmtFlags_ != nullptr);
         auto lmtFlags = ReadOrRaise<uint8_t>(rxbuf);
         this->FldDealLmtFlags_->PutNumber(aux.RawWr_, lmtFlags, 0);
      }

      auto     fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_;
      unsigned count = ReadOrRaise<byte>(rxbuf);
      if (rx.IsNeedsLog_) {
         RevPrint(rx.LogBuf_, "|rtDeal=", ToHex(dealFlags), "|count=", count + 1);
         rx.FlushLog();
      }
      if (count > 0) {
         this->FldDealFlags_->PutNumber(aux.RawWr_, static_cast<seed::FieldNumberT>(
            dealFlags - (f9sv_DealFlag_DealBuyCntChanged | f9sv_DealFlag_DealSellCntChanged)), 0);
         this->DecodeDealPQ(rxbuf, aux.RawWr_, pTotalQty);
         if (fnOnReport)
            fnOnReport(&rx.Session_, &rpt);
         dealFlags -= (f9sv_DealFlag_TotalQtyLost | f9sv_DealFlag_DealTimeChanged);
         if (--count > 0) {
            this->FldDealFlags_->PutNumber(aux.RawWr_, static_cast<seed::FieldNumberT>(
               dealFlags - (f9sv_DealFlag_DealBuyCntChanged | f9sv_DealFlag_DealSellCntChanged)), 0);
            do {
               this->DecodeDealPQ(rxbuf, aux.RawWr_, pTotalQty);
               if (fnOnReport)
                  fnOnReport(&rx.Session_, &rpt);
            } while (--count > 0);
         }
      }
      this->FldDealFlags_->PutNumber(aux.RawWr_, static_cast<seed::FieldNumberT>(dealFlags), 0);
      this->DecodeDealPQ(rxbuf, aux.RawWr_, pTotalQty);
      this->DecodeDealCnt(rxbuf, aux.RawWr_, dealFlags);
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
   void DecodeDealCnt(DcQueue& rxbuf, seed::SimpleRawWr& dealwr, const f9sv_DealFlag dealFlags) {
      if (IsEnumContains(dealFlags, f9sv_DealFlag_DealBuyCntChanged))
         this->FldDealBuyCnt_->BitvToCell(dealwr, rxbuf);
      if (IsEnumContains(dealFlags, f9sv_DealFlag_DealSellCntChanged))
         this->FldDealSellCnt_->BitvToCell(dealwr, rxbuf);
   }
   void DecodeDealBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      InfoAux        auxDeal{rx, rpt, rxbuf, this->TabIdxDeal_};
      f9sv_DealFlag  dflag = this->DecodeDealPack(std::move(auxDeal), rx, rpt, rxbuf);
      if (dflag != f9sv_DealFlag_IsOld)
         this->DecodeSnapshotBS(InfoAux{auxDeal, rpt, this->TabIdxBS_},
                                rx, rpt, rxbuf, (IsEnumContains(dflag, f9sv_DealFlag_Calculated)
                                                 ? f9sv_BSFlag_Calculated : f9sv_BSFlag{}));
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
   void DecodeSnapshotBS(InfoAux&& aux, svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf, f9sv_BSFlag bsFlags) {
      aux.PutDecField(*this->FldBSInfoTime_, *aux.SeInfoTime_);
      const FieldPQList* flds;
      while (!rxbuf.empty()) {
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, '}');
         const uint8_t bstype = ReadOrRaise<uint8_t>(rxbuf);
         static_assert((cast_to_underlying(fmkt::RtBSType::Mask) & 0x80) == 0, "");
         if ((bstype & 0x80) == 0) {
            switch (static_cast<fmkt::RtBSType>(bstype & cast_to_underlying(fmkt::RtBSType::Mask))) {
               #define case_RtBSType(type) case fmkt::RtBSType::type: \
                  flds = &this->Fld##type##s_;                        \
                  bsFlags |= f9sv_BSFlag_##type;                      \
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
            unsigned count = static_cast<unsigned>(bstype & 0x0f) + 1;
            auto     ibeg = flds->cbegin() + count;
            this->ClearFieldValues(aux.RawWr_, ibeg, flds->cend());
            do {
               --ibeg;
               ibeg->FldPri_->BitvToCell(aux.RawWr_, rxbuf);
               ibeg->FldQty_->BitvToCell(aux.RawWr_, rxbuf);
               if (fon9_UNLIKELY(rx.IsNeedsLog_)) {
                  RevPrintPQ(rx.LogBuf_, *ibeg, aux.RawWr_);
                  if (count > 1)
                     RevPrint(rx.LogBuf_, '|');
               }
            } while (--count > 0);
         }
         else { // [BS快照] 的 特殊欄位更新.
            switch (static_cast<fmkt::RtBSSnapshotSpc>(bstype)) {
            case fmkt::RtBSSnapshotSpc::LmtFlags:
            {  // BSLmtFlags.
               assert(this->FldBSLmtFlags_ != nullptr);
               auto lmtFlags = ReadOrRaise<uint8_t>(rxbuf);
               this->FldBSLmtFlags_->PutNumber(aux.RawWr_, lmtFlags, 0);
               if (fon9_UNLIKELY(rx.IsNeedsLog_))
                  RevPrint(rx.LogBuf_, ToHex(lmtFlags));
            }
            break;
            } // switch(bstype)
         }
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, "|rtBS.", ToHex(bstype), "={");
      }
      // -----
      #define check_ClearFieldValues(type) \
      while (!IsEnumContains(bsFlags, f9sv_BSFlag_##type)) { \
         this->ClearFieldValues(aux.RawWr_, this->Fld##type##s_.cbegin(), this->Fld##type##s_.cend()); \
         break; \
      }
      // -----
      check_ClearFieldValues(OrderBuy);
      check_ClearFieldValues(OrderSell);
      check_ClearFieldValues(DerivedBuy);
      check_ClearFieldValues(DerivedSell);
      // -----
      this->ReportBS(rx, rpt, aux.RawWr_, bsFlags);
   }
   void ReportBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, const seed::RawWr& wr, f9sv_BSFlag bsFlags) {
      this->FldBSFlags_->PutNumber(wr, static_cast<seed::FieldNumberT>(bsFlags), 0);
      this->ReportEv(rx, rpt);
   }
   static void ReportEv(svc::RxSubrData& rx, f9sv_ClientReport& rpt) {
      rx.FlushLog();
      if (auto fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_)
         fnOnReport(&rx.Session_, &rpt);
   }
   // -----
   bool IsBSMktSeqNewer(const InfoAux& aux, svc::RxSubrData& rx, DcQueue& rxbuf) const {
      if (!this->FldBSMktSeq_)
         return true;
      return aux.Note_.IsRxMktSeqNewer(*this->FldBSMktSeq_, aux.RawWr_, rx, rxbuf);
   }
   void DecodeUpdateBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      InfoAux  aux(rx, rpt, rxbuf, this->TabIdxBS_);
      if (!this->IsBSMktSeqNewer(aux, rx, rxbuf))
         return;
      aux.PutDecField(*this->FldBSInfoTime_, *aux.SeInfoTime_);
      const uint8_t first = ReadOrRaise<uint8_t>(rxbuf);
      f9sv_BSFlag   bsFlags{};
      if (first & 0x80)
         bsFlags |= f9sv_BSFlag_Calculated;
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
         const auto iLv = flds->cbegin() + (bsType & 0x0f);
         switch (static_cast<fmkt::RtBSAction>(bsType & cast_to_underlying(fmkt::RtBSAction::Mask))) {
         case fmkt::RtBSAction::New:
            InsertPQ(aux.RawWr_, iLv, flds->cend());
            /* fall through */ // 繼續取出 rxbuf 裡面的 Pri,Qty; 填入 iLv;
         case fmkt::RtBSAction::ChangePQ:
            iLv->FldPri_->BitvToCell(aux.RawWr_, rxbuf);
            iLv->FldQty_->BitvToCell(aux.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_))
               RevPrintPQ(rx.LogBuf_, *iLv, aux.RawWr_);
            break;
         case fmkt::RtBSAction::ChangeQty:
            iLv->FldQty_->BitvToCell(aux.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_))
               iLv->FldQty_->CellRevPrint(aux.RawWr_, nullptr, rx.LogBuf_);
            break;
         case fmkt::RtBSAction::Delete:
            DeletePQ(aux.RawWr_, iLv, flds->cend());
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
      this->ReportBS(rx, rpt, aux.RawWr_, bsFlags);
   }
   // -----
   bool IsIdxDealMktSeqNewer(const InfoAux& aux, svc::RxSubrData& rx, DcQueue& rxbuf) const {
      if (!this->FldIdxDealMktSeq_)
         return true;
      return aux.Note_.IsRxMktSeqNewer(*this->FldIdxDealMktSeq_, aux.RawWr_, rx, rxbuf);
   }
   static void BitvToIndexValue(const seed::Field& fld, seed::RawWr& wr, DcQueue& rxbuf) {
      fon9_BitvNumR numr;
      BitvToNumber(rxbuf, numr);
      if (numr.Type_ == fon9_BitvNumT_Null)
         fld.SetNull(wr);
      else
         fld.PutNumber(wr, signed_cast(numr.Num_), static_cast<DecScaleT>(numr.Scale_ + 2));
   }
   void DecodeIndexValueV2(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      InfoAux  aux{rx, rpt, rxbuf, this->TabIdxDeal_};
      if (!this->IsIdxDealMktSeqNewer(aux, rx, rxbuf))
         return;
      DayTime  dealTime = *aux.SeInfoTime_;
      aux.PutDecField(*this->FldDealTime_, dealTime);
      BitvToIndexValue(*this->FldDealPri_, aux.RawWr_, rxbuf);
      if (rx.IsNeedsLog_) {
         this->FldDealPri_->CellRevPrint(aux.RawWr_, nullptr, rx.LogBuf_);
         RevPrint(rx.LogBuf_, "|infoTime=", dealTime, "|value=");
         rx.FlushLog();
      }
      if (auto fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_)
         fnOnReport(&rx.Session_, &rpt);
      assert(rxbuf.empty());
   }
   void DecodeIndexValueV2List(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      InfoAux  aux{rx, rpt, rxbuf, this->TabIdxDeal_};
      if (!this->IsIdxDealMktSeqNewer(aux, rx, rxbuf))
         return;
      const auto  fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_;
      DayTime     dealTime = *aux.SeInfoTime_;
      aux.PutDecField(*this->FldDealTime_, dealTime);
      CharVector  tempKeyText;
      while (!rxbuf.empty()) {
         this->FetchSeedKey(*rx.SubrRec_, rxbuf, tempKeyText, rpt);
         seed::SimpleRawWr wr{SetRptTabSeed(rpt, this->TabIdxDeal_)};
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
   void DecodeTradingSessionId(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      InfoAux  aux(rx, rpt, rxbuf, this->TabIdxBase_);
      uint32_t tdayYYYYMMDD = ReadOrRaise<uint32_t>(rxbuf);
      char     sesId = cast_to_underlying(ReadOrRaise<f9fmkt_TradingSessionId>(rxbuf));
      auto     sesSt = ReadOrRaise<f9fmkt_TradingSessionSt>(rxbuf);
      this->FldBaseTDay_->PutNumber(aux.RawWr_, tdayYYYYMMDD, 0);
      this->FldBaseSession_->StrToCell(aux.RawWr_, StrView{&sesId, 1});
      this->FldBaseSessionSt_->PutNumber(aux.RawWr_, sesSt, 0);
      assert(rxbuf.empty());
      this->ReportEv(rx, rpt);
      /// 收到 TradingSession異動 或 DailyClear 事件, 必須重設 MktSeq, 因為交易所序號會重編;
      if (this->FldBSMktSeq_ && rx.NotifyKind_ == seed::SeedNotifyKind::StreamData)
         aux.Note_.ResetMktSeq(0);
   }
   void DecodeBaseInfoTw(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      InfoAux  auxBase(rx, rpt, rxbuf, this->TabIdxBase_);

      char  chTradingMarket = cast_to_underlying(ReadOrRaise<f9fmkt_TradingMarket>(rxbuf));
      this->FldBaseMarket_->StrToCell(auxBase.RawWr_, StrView{&chTradingMarket, 1});

      if (this->FldBaseFlowGroup_) {
         uint8_t  u8FlowGroup = ReadOrRaise<uint8_t>(rxbuf);
         this->FldBaseFlowGroup_->PutNumber(auxBase.RawWr_, u8FlowGroup, 0);
      }
      if (this->FldBaseStrikePriceDiv_) {
         uint8_t  u8StrikePriceDecimalLocator = ReadOrRaise<uint8_t>(rxbuf);
         this->FldBaseStrikePriceDiv_->PutNumber(auxBase.RawWr_, static_cast<seed::FieldNumberT>(
            GetDecDivisor(u8StrikePriceDecimalLocator)), 0);
      }
      if (this->FldBasePriceOrigDiv_) {
         uint8_t  u8PriceOrigDiv = ReadOrRaise<uint8_t>(rxbuf);
         this->FldBasePriceOrigDiv_->PutNumber(auxBase.RawWr_, static_cast<seed::FieldNumberT>(
            GetDecDivisor(static_cast<DecScaleT>(this->FldPriRef_->DecScale_ - u8PriceOrigDiv))), 0);
         auxBase.Note_.SetPriScale(u8PriceOrigDiv);
      }
      if (this->FldBaseShUnit_) {
         this->FldBaseShUnit_->BitvToCell(auxBase.RawWr_, rxbuf);
      }
      this->ReportEv(rx, rpt);

      InfoAux  auxRef{auxBase, rpt, this->TabIdxRef_};
      this->FldPriRef_->BitvToCell(auxRef.RawWr_, rxbuf);
      if (rxbuf.empty())
         this->ReportEv(rx, rpt);
      else
         this->DecodePriLmts(std::move(auxRef), rx, rpt, rxbuf);
   }
   void DecodePriLmts(InfoAux&& auxRef, svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      this->FldPriUpLmt_->BitvToCell(auxRef.RawWr_, rxbuf);
      this->FldPriDnLmt_->BitvToCell(auxRef.RawWr_, rxbuf);
      for (auto& fldPriLmt : this->FldPriLmts_) {
         if (rxbuf.empty()) {
            fldPriLmt.Up_->SetNull(auxRef.RawWr_);
            fldPriLmt.Dn_->SetNull(auxRef.RawWr_);
         }
         else {
            fldPriLmt.Up_->BitvToCell(auxRef.RawWr_, rxbuf);
            fldPriLmt.Dn_->BitvToCell(auxRef.RawWr_, rxbuf);
         }
      }
      this->ReportEv(rx, rpt);
      assert(rxbuf.empty());
   }
   // -----
   static bool FieldBitvToCell(svc::RxSubrData& rx, DcQueue& rxbuf, const seed::Field* fld, seed::RawWr& wr) {
      assert(fld != nullptr);
      ByteVector old;
      fld->AppendRawBytes(wr, old);
      fld->BitvToCell(wr, rxbuf);
      bool isChanged = (fld->CompareRawBytes(wr, old.begin(), old.size()) != 0);
      if (fon9_UNLIKELY(rx.IsNeedsLog_)) {
         fld->CellRevPrint(wr, nullptr, rx.LogBuf_);
         if (!isChanged)
            RevPrint(rx.LogBuf_, '=');
         RevPrint(rx.LogBuf_, fld->Name_, '=');
         RevPrint(rx.LogBuf_, '|');
      }
      return isChanged;
   }
   void DecodeFieldValue_NoInfoTime(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      auto        fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_;
      NoteAux     aux{rx, rpt};
      auto        nTabFld = ReadOrRaise<uint8_t>(rxbuf);
      f9sv_Index  fldIdxList[32];
      rpt.StreamPackExArgs_ = reinterpret_cast<uintptr_t>(&fldIdxList[0]);
      while (!rxbuf.empty()) {
         auto const  tabidx = unsigned_cast(nTabFld >> 4);
         auto* const tab = rx.SubrRec_->Tree_->Layout_->GetTab(tabidx);
         assert(tab != nullptr);
         fldIdxList[0] = 0;
         do {
            seed::SimpleRawWr wr{SetRptTabSeed(rpt, tabidx)};
            const auto  fldidx = unsigned_cast(nTabFld & 0x0f);
            if (this->FieldBitvToCell(rx, rxbuf, tab->Fields_.Get(fldidx), wr))
               fldIdxList[++fldIdxList[0]] = static_cast<f9sv_Index>(fldidx);
            if (rxbuf.empty())
               break;
            nTabFld = ReadOrRaise<uint8_t>(rxbuf);
         } while (tabidx == unsigned_cast(nTabFld >> 4));
         if (fon9_UNLIKELY(rx.IsNeedsLog_)) {
            RevPrint(rx.LogBuf_, "|tabidx=", tabidx);
            rx.FlushLog();
         }
         if (fnOnReport && fldIdxList[0] > 0)
            fnOnReport(&rx.Session_, &rpt);
      }
   }
   void DecodeFieldValue_AndInfoTime(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      auto* note = static_cast<RcSvStreamDecoderNote_MdRts*>(rx.SubrSeedRec_->StreamDecoderNote_.get());
      BitvToDayTimeOrUnchange(rxbuf, *note->SelectInfoTime(rx));
      DecodeFieldValue_NoInfoTime(rx, rpt, rxbuf);
   }
   void DecodeTabValues_NoInfoTime(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      auto     fnOnReport = rx.SubrSeedRec_->Handler_.FnOnReport_;
      NoteAux  aux{rx, rpt};
      while (!rxbuf.empty()) {
         auto  tabidx = ReadOrRaise<uint8_t>(rxbuf);
         auto* tab = rx.SubrRec_->Tree_->Layout_->GetTab(tabidx);
         assert(tab != nullptr);
         seed::SimpleRawWr wr{SetRptTabSeed(rpt, tabidx)};
         const auto fldCount = tab->Fields_.size();
         for (unsigned fldidx = 0; fldidx < fldCount; ++fldidx)
            FieldBitvToCell(rx, rxbuf, tab->Fields_.Get(fldidx), wr);
         if (fon9_UNLIKELY(rx.IsNeedsLog_)) {
            RevPrint(rx.LogBuf_, "|tabidx=", tabidx);
            rx.FlushLog();
         }
         if (fnOnReport)
            fnOnReport(&rx.Session_, &rpt);
      }
   }
   void DecodeTabValues_AndInfoTime(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      auto* note = static_cast<RcSvStreamDecoderNote_MdRts*>(rx.SubrSeedRec_->StreamDecoderNote_.get());
      BitvToDayTimeOrUnchange(rxbuf, *note->SelectInfoTime(rx));
      DecodeTabValues_NoInfoTime(rx, rpt, rxbuf);
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
