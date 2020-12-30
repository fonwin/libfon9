// \file fon9/rc/RcMdRtsDecoder.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcMdRtsDecoder_hpp__
#define __fon9_rc_RcMdRtsDecoder_hpp__
#include "fon9/rc/RcMdRtsDecoder.h"
#include "fon9/rc/RcSvStreamDecoder.hpp"
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/seed/RawWr.hpp"

namespace fon9 { namespace rc {

inline f9sv_Seed* SetRptTabSeed(f9sv_ClientReport& rpt, f9sv_TabSize tabidx) {
   rpt.Tab_  = &rpt.Layout_->TabArray_[tabidx];
   return const_cast<f9sv_Seed*>(rpt.Seed_ = rpt.SeedArray_[tabidx]);
}

struct RcMdRtsDecoder_TabFields_POD {
   f9sv_TabSize         TabIdxBS_;
   f9sv_TabSize         TabIdxDeal_;
   f9sv_TabSize         TabIdxBase_;
   f9sv_TabSize         TabIdxRef_;

   const seed::Field*   FldBaseTDay_;
   const seed::Field*   FldBaseSession_;
   const seed::Field*   FldBaseSessionSt_;
   const seed::Field*   FldBaseMarket_;
   const seed::Field*   FldBaseFlowGroup_;
   const seed::Field*   FldBasePriceOrigDiv_;
   const seed::Field*   FldBaseStrikePriceDiv_;
   const seed::Field*   FldBaseShUnit_;
   const seed::Field*   FldBaseTwsFlags_;

   const seed::Field*   FldPriRef_;
   const seed::Field*   FldPriUpLmt_;
   const seed::Field*   FldPriDnLmt_;
   const seed::Field*   FldLvUpLmt_;
   const seed::Field*   FldLvDnLmt_;

   const seed::Field*   FldDealInfoTime_;
   const seed::Field*   FldDealTime_;
   const seed::Field*   FldDealTotalQty_;
   const seed::Field*   FldDealPri_;
   const seed::Field*   FldDealQty_;
   const seed::Field*   FldDealBuyCnt_;
   const seed::Field*   FldDealSellCnt_;
   const seed::Field*   FldDealFlags_;
   const seed::Field*   FldDealLmtFlags_;
   const seed::Field*   FldIdxDealMktSeq_;

   const seed::Field*   FldBSInfoTime_;
   const seed::Field*   FldBSFlags_;
   const seed::Field*   FldBSLmtFlags_;
   const seed::Field*   FldBSMktSeq_;

   void RcMdRtsDecoder_TabFields_POD_ctor() {
      static_assert(std::is_pod<RcMdRtsDecoder_TabFields_POD>::value, "");
      ZeroStruct(static_cast<RcMdRtsDecoder_TabFields_POD*>(this));
   }
};

struct RcMdRtsDecoder_TabFields : public RcMdRtsDecoder_TabFields_POD {
   struct FieldPQ {
      const seed::Field*   FldPri_;
      const seed::Field*   FldQty_;

      void CopyFrom(const FieldPQ& src, const seed::RawWr& bsWr) const {
         assert(src.FldPri_->Size_ == this->FldPri_->Size_ && src.FldPri_->DecScale_ == this->FldPri_->DecScale_);
         assert(src.FldQty_->Size_ == this->FldQty_->Size_);
         memcpy(bsWr.GetCellPtr<char>(*this->FldPri_), bsWr.GetCellPtr<char>(*src.FldPri_), src.FldPri_->Size_);
         memcpy(bsWr.GetCellPtr<char>(*this->FldQty_), bsWr.GetCellPtr<char>(*src.FldQty_), src.FldQty_->Size_);
      }
   };
   using FieldPQList = std::vector<FieldPQ>;
   FieldPQList FldOrderBuys_;
   FieldPQList FldOrderSells_;
   FieldPQList FldDerivedBuys_;
   FieldPQList FldDerivedSells_;
   static void SetBSFields(FieldPQList& dst, const seed::Fields& flds, char ch1, char ch2);

   struct FieldPriLmt {
      const seed::Field*   Up_;
      const seed::Field*   Dn_;
   };
   using FieldPriLmts = std::vector<FieldPriLmt>;
   FieldPriLmts FldPriLmts_;

   RcMdRtsDecoder_TabFields(svc::TreeRec& tree);

   static void InsertPQ(const seed::RawWr& bsWr,
                        FieldPQList::const_iterator iLv,
                        FieldPQList::const_iterator iEnd) {
      for (--iEnd; iEnd != iLv; --iEnd)
         iEnd->CopyFrom(*(iEnd - 1), bsWr);
   }
   static void DeletePQ(const seed::RawWr& bsWr,
                        FieldPQList::const_iterator iLv,
                        FieldPQList::const_iterator iEnd) {
      for (--iEnd; iLv != iEnd; ++iLv)
         iLv->CopyFrom(*(iLv + 1), bsWr);
      iEnd->FldPri_->SetNull(bsWr);
      iEnd->FldQty_->SetNull(bsWr);
   }
};

class RcMdRtsDecoder : public RcSvStreamDecoder, public RcMdRtsDecoder_TabFields {
   fon9_NON_COPY_NON_MOVE(RcMdRtsDecoder);
public:
   /// 直接使用 RcMdRtsDecoder_TabFields 的建構.
   using RcMdRtsDecoder_TabFields::RcMdRtsDecoder_TabFields;
};

class RcMdRtsDecoderNote : public RcSvStreamDecoderNote {
   fon9_NON_COPY_NON_MOVE(RcMdRtsDecoderNote);

   uint64_t    RtMktSeq_{0};
   DecScaleT   PriScale_{0};
   char        Padding____[7];

public:
   RcMdRtsDecoderNote() = default;

   DecScaleT GetPriScale() const {
      return this->PriScale_;
   }
   void SetPriScale(DecScaleT v) {
      this->PriScale_ = v;
   }

   /// - 訂閱成功取得商品 Snapshot 時, 更新現在的 MktSeq.
   /// - 每日清檔時.
   /// - 換盤時(日盤 => 夜盤).
   void ResetMktSeq(uint64_t mktseq) {
      this->RtMktSeq_ = mktseq;
   }
   bool IsMktSeqNewer(const seed::Field& fldBSMktSeq, const seed::RawWr& bsWr, uint64_t rxSeq) {
      if (fon9_LIKELY(this->RtMktSeq_ < rxSeq || rxSeq == 0)) {
         fldBSMktSeq.PutNumber(bsWr, signed_cast(this->RtMktSeq_ = rxSeq), 0);
         return true;
      }
      return false;
   }
   bool IsMktSeqNext(uint64_t rxSeq) {
      if (rxSeq != this->RtMktSeq_ + 1)
         return false;
      this->RtMktSeq_ = rxSeq;
      return true;
   }
   bool IsMktSeqNext(const seed::Field& fldBSMktSeq, const seed::RawWr& bsWr, uint64_t rxSeq) {
      if (rxSeq != this->RtMktSeq_ + 1)
         return false;
      fldBSMktSeq.PutNumber(bsWr, signed_cast(this->RtMktSeq_ = rxSeq), 0);
      return true;
   }
};

} } // namespaces
#endif//__fon9_rc_RcMdRtsDecoder_hpp__
