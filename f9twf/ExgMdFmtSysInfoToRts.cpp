// \file f9twf/ExgMdFmtSysInfoToRts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtSysInfoToRts.hpp"
#include "f9twf/ExgMdFmtSysInfo.hpp"
#include "f9twf/ExgMdFmtParsers.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

struct I140_CheckPublish {
   fon9_NON_COPY_NON_MOVE(I140_CheckPublish);
   fon9::RevBufferFixedSize<128> Rts_;
   I140_CheckPublish() = default;
   virtual ~I140_CheckPublish() {
   }
   virtual void CheckPublish(ExgMdSymb& symb) = 0;
};
inline f9fmkt_TradingMarket GetPkMarket(const ExgMdHead0& pk) {
   assert(pk.MessageKind_ == '3');
   return (pk.TransmissionCode_ == '2'
           ? f9fmkt_TradingMarket_TwFUT
           : f9fmkt_TradingMarket_TwOPT);
}
//--------------------------------------------------------------------------//
static void PublishSymbolList(const ExgMdSymbs::Locker& lk, I140_CheckPublish& dst,
                              unsigned count, const char* pProdId, const unsigned szProdId) {
   while (count > 0) {
      const char*   peos = static_cast<const char*>(memchr(pProdId, ' ', szProdId));
      fon9::StrView symbId{pProdId, peos ? peos : (pProdId + szProdId)};
      if (auto symb = static_cast<ExgMdSymb*>(ExgMdSymbs::GetSymb(lk, symbId).get()))
         dst.CheckPublish(*symb);
      pProdId += szProdId;
      --count;
   }
}
static void PublishContractList(ExgMdSymbs& symbs, I140_CheckPublish& dst,
                                unsigned count, const ExgMdSysInfo_ContractId* pConId3) {
   while (count > 0) {
      auto& con = symbs.FetchContract(ContractId{pConId3->Chars_, 3});
      for (auto* psymb : con.Symbs_)
         dst.CheckPublish(*psymb);
      ++pConId3;
      --count;
   }
}
static void PublishOptYMList(ExgMdSymbs& symbs, I140_CheckPublish& dst,
                            unsigned count, const ExgMdSysInfo_OptYM* pOptYM) {
   CodeMY idC;
   while (count > 0) {
      unsigned yyyymm = fon9::StrTo(fon9::StrView(pOptYM->Chars_ + 3, pOptYM->end()), 0u);
      if (idC.FromYYYYMM(yyyymm)) {
         const char chPut = static_cast<char>(idC.MY_[0] + 12);
         auto&      con = symbs.FetchContract(ContractId{pOptYM->Chars_, 3});
         for (auto* psymb : con.Symbs_) {
            if (psymb->SymbId_.size() >= 5) {
               const char* pSymbMY = psymb->SymbId_.end() - 2;
               if (pSymbMY[1] == idC.MY_[1])
                  if (pSymbMY[0] == idC.MY_[0] || pSymbMY[0] == chPut)
                     dst.CheckPublish(*psymb);
            }
         }
      }
      ++pOptYM;
      --count;
   }
}
//--------------------------------------------------------------------------//
void CheckIdList(ExgMdSymbs& symbs, const ExgMdSymbs::Locker& lk, I140_CheckPublish& dst,
                 ExgMdSysInfo_ListType lt, const ExgMdSysInfo_IdList& list) {
   switch (lt) {
   default:
   case ExgMdSysInfo_ListType::All:
   case ExgMdSysInfo_ListType::FlowGroup:
   case ExgMdSysInfo_ListType::OptYYYYMM:
      break;
   case ExgMdSysInfo_ListType::Symbol:
      PublishSymbolList(lk, dst, fon9::PackBcdTo<unsigned>(list.Count_),
                        list.List_->ProdId10_.Chars_, sizeof(list.List_->ProdId10_));
      break;
   case ExgMdSysInfo_ListType::Contract:
      PublishContractList(symbs, dst, fon9::PackBcdTo<unsigned>(list.Count_),
                          &list.List_->ContractId3_);
      break;
   } // switch(list type.
}
//--------------------------------------------------------------------------//
struct I140_BreakSt_PreparePk : public I140_CheckPublish {
   fon9_NON_COPY_NON_MOVE(I140_BreakSt_PreparePk);
   f9fmkt::SymbBreakSt           BreakSt_;
   fon9::StrView                 PkSessionSt_;
   fon9::StrView                 PkBreakSt_;
   const f9fmkt_TradingSessionSt TSessionSt_;
   char                          Padding___[7];
   const fon9::DayTime           InfoTime_;
   ExgMdSymbsMgr* const          SymbsMgr_;
   I140_BreakSt_PreparePk(const fon9::seed::Layout& layout, f9fmkt_TradingSessionSt st, ExgMcMessage& e)
      : TSessionSt_{st}
      , InfoTime_{e.Pk_.InformationTime_.ToDayTime()}
      , SymbsMgr_{e.Channel_.GetChannelMgr()} {
      const auto* fldSessionSt = layout.GetTab(fon9_kCSTR_TabName_Base)->Fields_.Get("SessionSt");
      fon9::ToBitv(this->Rts_, st);
      f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fldSessionSt);
      this->PkSessionSt_ = ToStrView(this->Rts_);
   }
   void SetupBreakSt(const fon9::seed::Layout& layout) {
      const auto* tabBreakSt = layout.GetTab(fon9_kCSTR_TabName_BreakSt);
      fon9::seed::SimpleRawRd rd{this->BreakSt_};
      for (auto L = tabBreakSt->Fields_.size(); L > 0;) {
         const auto* fld = tabBreakSt->Fields_.Get(--L);
         fld->CellToBitv(rd, this->Rts_);
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);
      }
      this->PkBreakSt_.Reset(this->Rts_.GetCurrent(), this->PkSessionSt_.begin());
   }
   void CheckPublish(ExgMdSymb& symb) override {
      if (this->SymbsMgr_ && !this->SymbsMgr_->CheckSymbTradingSessionId(symb))
         return;
      fon9::RevBufferList  rts{64};
      f9sv_MdRtsKind       pkKind = f9sv_MdRtsKind_TradingSession;
      if (symb.TradingSessionSt_ == this->TSessionSt_) {
         if (symb.BreakSt_.Data_ == this->BreakSt_.Data_)
            return;
         fon9::RevPrint(rts, this->PkBreakSt_);
         symb.BreakSt_.Data_ = this->BreakSt_.Data_;
         pkKind = f9sv_MdRtsKind_All_AndInfoTime;
      }
      else {
         symb.TradingSessionSt_ = this->TSessionSt_;
         if (symb.BreakSt_.Data_ == this->BreakSt_.Data_)
            fon9::RevPrint(rts, this->PkSessionSt_);
         else {
            fon9::RevPutMem(rts, this->Rts_.GetCurrent(), this->Rts_.GetMemEnd());
            symb.BreakSt_.Data_ = this->BreakSt_.Data_;
            pkKind = f9sv_MdRtsKind_All_AndInfoTime;
         }
      }
      symb.MdRtStream_.Publish(ToStrView(symb.SymbId_),
                               f9sv_RtsPackType_FieldValue_AndInfoTime,
                               pkKind,
                               this->InfoTime_, std::move(rts));
   }
};
static void I140_200_TradingSessionSt(ExgMcMessage& e) {
   const auto& i200 = I140CastTo<ExgMdSysInfo200>(*static_cast<const ExgMcI140*>(&e.Pk_));
   auto&       symbs = *e.Channel_.GetChannelMgr()->Symbs_;

   I140_BreakSt_PreparePk  ppk{*symbs.LayoutSP_, f9fmkt_TradingSessionSt_Halted, e};
   ppk.BreakSt_.Data_.Reason_ = i200.Reason_;
   ppk.BreakSt_.Data_.BreakHHMMSS_ = fon9::PackBcdTo<uint32_t>(i200.BreakHHMMSS_);
   ppk.SetupBreakSt(*symbs.LayoutSP_);
   CheckIdList(symbs, symbs.SymbMap_.Lock(), ppk, i200.ListType_, i200.IdList_);
};
static void I140_201_TradingSessionSt(ExgMcMessage& e) {
   const auto& i201 = I140CastTo<ExgMdSysInfo201>(*static_cast<const ExgMcI140*>(&e.Pk_));
   auto&       symbs = *e.Channel_.GetChannelMgr()->Symbs_;

   I140_BreakSt_PreparePk  ppk{*symbs.LayoutSP_, f9fmkt_TradingSessionSt_Open, e};
   ppk.BreakSt_.Data_.Reason_ = i201.Reason_;
   ppk.BreakSt_.Data_.ReopenHHMMSS_ = fon9::PackBcdTo<uint32_t>(i201.ReopenHHMMSS_);
   ppk.BreakSt_.Data_.RestartHHMMSS_ = fon9::PackBcdTo<uint32_t>(i201.StartHHMMSS_);
   ppk.SetupBreakSt(*symbs.LayoutSP_);
   CheckIdList(symbs, symbs.SymbMap_.Lock(), ppk, i201.ListType_, i201.IdList_);
};
//--------------------------------------------------------------------------//
static void I140_30x_TradingSessionSt(ExgMcMessage& e, f9fmkt_TradingSessionSt st) {
   const auto& i30x = I140CastTo<ExgMdSysInfo30x>(*static_cast<const ExgMcI140*>(&e.Pk_));
   auto&       symbs = *e.Channel_.GetChannelMgr()->Symbs_;

   I140_BreakSt_PreparePk  ppk{*symbs.LayoutSP_, st, e};
   ppk.BreakSt_.Data_.Reason_ = i30x.Reason_;
   ppk.SetupBreakSt(*symbs.LayoutSP_);

   ExgMdSymbs::Locker   lk{symbs.SymbMap_.Lock()};
   if (i30x.ListType_ == ExgMdSysInfo_ListType::FlowGroup) {
      const auto flowGroup = fon9::PackBcdTo<f9twf::SymbFlowGroup_t>(i30x.List_.FlowGroup_);
      const auto mkt = GetPkMarket(e.Pk_);
      for (auto& isymb : *lk) {
         ExgMdSymb& symb = *static_cast<ExgMdSymb*>(isymb.second.get());
         if (symb.FlowGroup_ == flowGroup && symb.TradingMarket_ == mkt)
            ppk.CheckPublish(symb);
      }
   }
   else {
      CheckIdList(symbs, lk, ppk, i30x.ListType_, i30x.List_.IdList_);
   }
};
//--------------------------------------------------------------------------//
struct I140_DynBand_PreparePk : public I140_CheckPublish {
   fon9_NON_COPY_NON_MOVE(I140_DynBand_PreparePk);
   f9fmkt::SymbDynBand  DynBand_;
   I140_DynBand_PreparePk() = default;
   void CheckList(ExgMdSymbs& symbs, ExgMdSysInfo_ListType lt, const ExgMdSysInfo_IdListL& list,
                  f9fmkt_TradingMarket mkt) {
      ExgMdSymbs::Locker lk{symbs.SymbMap_.Lock()};
      switch (lt) {
      default:
      case ExgMdSysInfo_ListType::FlowGroup:
         // 動態價格穩定措施.沒有這些 ListType.
         break;
      case ExgMdSysInfo_ListType::Symbol:
         PublishSymbolList(lk, *this, fon9::PackBcdTo<unsigned>(list.Count_),
                           list.List_->ProdId20_.Chars_, sizeof(list.List_->ProdId20_));
         break;
      case ExgMdSysInfo_ListType::Contract:
         PublishContractList(symbs, *this,
                             fon9::PackBcdTo<unsigned>(list.Count_),
                             &list.List_->ContractId3_);
         break;
      case ExgMdSysInfo_ListType::OptYYYYMM:
         PublishOptYMList(symbs, *this,
                          fon9::PackBcdTo<unsigned>(list.Count_),
                          &list.List_->OptYYYYMM_);
         break;
      case ExgMdSysInfo_ListType::All:
         for (auto& isymb : *lk) {
            ExgMdSymb& symb = *static_cast<ExgMdSymb*>(isymb.second.get());
            if (symb.TradingMarket_ == mkt)
               this->CheckPublish(symb);
         }
         break;
      } // switch(list type.
   }
};
static void I140_40r_DynRange(ExgMcMessage& e, f9fmkt_DynBandSt st) {
   const auto& i40r = I140CastTo<ExgMdSysInfo40r>(*static_cast<const ExgMcI140*>(&e.Pk_));
   auto&       symbs = *e.Channel_.GetChannelMgr()->Symbs_;

   struct PreparePk : public I140_DynBand_PreparePk {
      fon9_NON_COPY_NON_MOVE(PreparePk);
      PreparePk(const fon9::seed::Layout& layout, f9fmkt_DynBandSt st, const ExgMdSysInfo40r& i40r, fon9::DayTime infoTime) {
         const auto* tab = layout.GetTab(fon9_kCSTR_TabName_DynBand);
         const auto* fld = tab->Fields_.Get("St");
         ToBitv(this->Rts_, this->DynBand_.Data_.DynBandSt_ = st);
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);

         fld = tab->Fields_.Get("Time");
         ToBitv(this->Rts_, this->DynBand_.Data_.StHHMMSS_ = fon9::PackBcdTo<uint32_t>(i40r.HHMMSS_));
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);

         fld = tab->Fields_.Get("Reason");
         ToBitv(this->Rts_, this->DynBand_.Data_.Reason_ = f9fmkt_DynBandStopReason_None);
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);

         fld = tab->Fields_.Get("SideType");
         ToBitv(this->Rts_, this->DynBand_.Data_.SideType_ = i40r.SideType_);
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);

         switch (i40r.SideType_) {
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_All:
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_Long:
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_OptLong:
            this->DynBand_.Data_.RangeL_.Assign<1>(fon9::PackBcdTo<uint8_t>(i40r.Range9V9_));
            fld = tab->Fields_.Get("RangeL");
            ToBitv(this->Rts_, this->DynBand_.Data_.RangeL_);
            f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);
            if (i40r.SideType_ != f9fmkt_DynBandSide::f9fmkt_DynBandSide_All)
               break;
            // i40r.SideType_ == f9fmkt_DynBandSide::f9fmkt_DynBandSide_All
            /* fall through */ // 因為是 f9fmkt_DynBandSide_All, 所以繼續處理 RangeS;
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_Short:
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_OptShort:
            this->DynBand_.Data_.RangeS_.Assign<1>(fon9::PackBcdTo<uint8_t>(i40r.Range9V9_));
            fld = tab->Fields_.Get("RangeS");
            ToBitv(this->Rts_, this->DynBand_.Data_.RangeS_);
            f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);
            break;
         }
         ToBitv(this->Rts_, infoTime);
      }
      void CheckPublish(ExgMdSymb& symb) override {
         if (this->DynBand_.Data_.IsEqualExcludeRange(symb.DynBand_.Data_)) {
            switch (this->DynBand_.Data_.SideType_) {
            case f9fmkt_DynBandSide::f9fmkt_DynBandSide_All:
               if (this->DynBand_.Data_.RangeL_ == symb.DynBand_.Data_.RangeL_
                   && this->DynBand_.Data_.RangeS_ == symb.DynBand_.Data_.RangeS_)
                  return;
               break;
            case f9fmkt_DynBandSide::f9fmkt_DynBandSide_Long:
            case f9fmkt_DynBandSide::f9fmkt_DynBandSide_OptLong:
               if (this->DynBand_.Data_.RangeL_ == symb.DynBand_.Data_.RangeL_)
                  return;
               break;
            case f9fmkt_DynBandSide::f9fmkt_DynBandSide_Short:
            case f9fmkt_DynBandSide::f9fmkt_DynBandSide_OptShort:
               if (this->DynBand_.Data_.RangeS_ == symb.DynBand_.Data_.RangeS_)
                  return;
               break;
            }
         }
         symb.DynBand_.Data_.CopyExcludeRange(this->DynBand_.Data_);
         switch (this->DynBand_.Data_.SideType_) {
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_All:
            symb.DynBand_.Data_.RangeL_ = this->DynBand_.Data_.RangeL_;
            symb.DynBand_.Data_.RangeS_ = this->DynBand_.Data_.RangeS_;
            break;
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_Long:
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_OptLong:
            symb.DynBand_.Data_.RangeL_ = this->DynBand_.Data_.RangeL_;
            break;
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_Short:
         case f9fmkt_DynBandSide::f9fmkt_DynBandSide_OptShort:
            symb.DynBand_.Data_.RangeS_ = this->DynBand_.Data_.RangeS_;
            break;
         }
         fon9::RevBufferList  rts{64};
         fon9::RevPutMem(rts, this->Rts_.GetCurrent(), this->Rts_.GetMemEnd());
         // 在建構時, this->Rts_ 有填入 InfoTime, 所以這裡使用 FieldValue_AndInfoTime;
         symb.MdRtStream_.PublishAndSave(ToStrView(symb.SymbId_),
                                         f9sv_RtsPackType_FieldValue_AndInfoTime,
                                         f9sv_MdRtsKind_DynBand,
                                         std::move(rts));
      }
   };
   PreparePk ppk{*symbs.LayoutSP_, st, i40r, e.Pk_.InformationTime_.ToDayTime()};
   ppk.CheckList(symbs, i40r.ListType_, i40r.List_, GetPkMarket(e.Pk_));
};
static void I140_40x_DynBand(ExgMcMessage& e, f9fmkt_DynBandSt st) {
   const auto& i40x = I140CastTo<ExgMdSysInfo40x>(*static_cast<const ExgMcI140*>(&e.Pk_));
   auto&       symbs = *e.Channel_.GetChannelMgr()->Symbs_;

   struct PreparePk : public I140_DynBand_PreparePk {
      fon9_NON_COPY_NON_MOVE(PreparePk);
      PreparePk(const fon9::seed::Layout& layout, f9fmkt_DynBandSt st, const ExgMdSysInfo40x& i40x, fon9::DayTime infoTime) {
         const auto* tab = layout.GetTab(fon9_kCSTR_TabName_DynBand);
         const auto* fld = tab->Fields_.Get("St");
         ToBitv(this->Rts_, this->DynBand_.Data_.DynBandSt_ = st);
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);

         fld = tab->Fields_.Get("Time");
         ToBitv(this->Rts_, this->DynBand_.Data_.StHHMMSS_ = fon9::PackBcdTo<uint32_t>(i40x.HHMMSS_));
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);

         fld = tab->Fields_.Get("Reason");
         ToBitv(this->Rts_, this->DynBand_.Data_.Reason_ = i40x.Reason_);
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fld);

         ToBitv(this->Rts_, infoTime);
      }
      void CheckPublish(ExgMdSymb& symb) override {
         if (this->DynBand_.Data_.IsEqualExcludeSide(symb.DynBand_.Data_))
            return;
         symb.DynBand_.Data_.CopyExcludeSide(this->DynBand_.Data_);
         fon9::RevBufferList  rts{64};
         fon9::RevPutMem(rts, this->Rts_.GetCurrent(), this->Rts_.GetMemEnd());
         // 在建構時, this->Rts_ 有填入 InfoTime, 所以這裡使用 FieldValue_AndInfoTime;
         symb.MdRtStream_.PublishAndSave(ToStrView(symb.SymbId_),
                                         f9sv_RtsPackType_FieldValue_AndInfoTime,
                                         f9sv_MdRtsKind_DynBand,
                                         std::move(rts));
      }
   };
   PreparePk ppk{*symbs.LayoutSP_, st, i40x, e.Pk_.InformationTime_.ToDayTime()};
   ppk.CheckList(symbs, i40x.ListType_, i40x.List_, GetPkMarket(e.Pk_));
};
//--------------------------------------------------------------------------//
static void I140_10x_LmtRange(ExgMcMessage& e, int lvWill) {
   const auto& i10x = I140CastTo<ExgMdSysInfo10x>(*static_cast<const ExgMcI140*>(&e.Pk_));
   auto&       symbs = *e.Channel_.GetChannelMgr()->Symbs_;
   struct PreparePk : public I140_CheckPublish {
      fon9_NON_COPY_NON_MOVE(PreparePk);
      int8_t   LvUpLmt_ = 0, LvDnLmt_ = 0;
      char     Padding___[6];
      PreparePk(const fon9::seed::Layout& layout, const ExgMdSysInfo10x& i10x, int lvWill, fon9::DayTime infoTime) {
         auto   lv = static_cast<uint8_t>(fon9::PackBcdTo<uint8_t>(i10x.Level_) - 1);
         assert(lv < TwfSymbRef_Data::kPriLmtCount);
         if (lv >= TwfSymbRef_Data::kPriLmtCount)
            lv = TwfSymbRef_Data::kPriLmtCount - 1;
         if (IsEnumContains(i10x.ExpandType_, ExgMdSysInfo_ExpandType::UpLmt))
            this->LvUpLmt_ = fon9::signed_cast(lv);
         if (IsEnumContains(i10x.ExpandType_, ExgMdSysInfo_ExpandType::DnLmt))
            this->LvDnLmt_ = fon9::signed_cast(lv);
         if (lvWill < 0) {
            this->LvUpLmt_ = static_cast<int8_t>(-this->LvUpLmt_);
            this->LvDnLmt_ = static_cast<int8_t>(-this->LvDnLmt_);
         }
         const auto* tabRef = layout.GetTab(fon9_kCSTR_TabName_Ref);
         assert(tabRef != nullptr);
         this->FieldToBitv(*tabRef, this->LvUpLmt_, "LvUpLmt");
         this->FieldToBitv(*tabRef, this->LvDnLmt_, "LvDnLmt");
         fon9::ToBitv(this->Rts_, infoTime);
      }
      void FieldToBitv(const fon9::seed::Tab& tabRef, int lvLmt, fon9::StrView fldName) {
         if (lvLmt == 0)
            return;
         const auto* fldLvLmt = tabRef.Fields_.Get(fldName);
         assert(fldLvLmt != nullptr);
         fon9::ToBitv(this->Rts_, lvLmt);
         f9fmkt::MdRtsPackFieldValueNid(this->Rts_, *fldLvLmt);
      }
      static bool CheckSetLvLmt(int8_t& dst, int8_t src) {
         if ((src == 0) || (dst == src))
            return false;
         dst = src;
         return true;
      }
      void CheckPublish(ExgMdSymb& symb) override {
         const bool isChanged = this->CheckSetLvLmt(symb.Ref_.Data_.LvUpLmt_, this->LvUpLmt_)
                              | this->CheckSetLvLmt(symb.Ref_.Data_.LvDnLmt_, this->LvDnLmt_);
         if (!isChanged)
            return;
         fon9::RevBufferList  rts{64};
         fon9::RevPutMem(rts, this->Rts_.GetCurrent(), this->Rts_.GetMemEnd());
         // 在建構時, this->Rts_ 有填入 InfoTime, 所以這裡使用 FieldValue_AndInfoTime;
         symb.MdRtStream_.PublishAndSave(ToStrView(symb.SymbId_),
                                         f9sv_RtsPackType_FieldValue_AndInfoTime,
                                         f9sv_MdRtsKind_Ref,
                                         std::move(rts));
      }
   };
   PreparePk ppk{*symbs.LayoutSP_, i10x, lvWill, e.Pk_.InformationTime_.ToDayTime()};
   CheckIdList(symbs, symbs.SymbMap_.Lock(), ppk, i10x.ListType_, i10x.IdList_);
}
//--------------------------------------------------------------------------//
f9twf_API void I140SysInfoParserToRts(ExgMcMessage& e) {
   const ExgMcI140& i140 = *static_cast<const ExgMcI140*>(&e.Pk_);
   if (0);// TODO: 如果有訂閱 tree(例:B主機 訂閱 A主機 的 /TwfMd/Symbs), 且 i140.ListType != Symbol, 要怎麼通知呢?
   // ?-1-? Add tree:「FlowGroup」, 可訂閱tree, 期貨、選擇權各一個 tree?
   // ?-2-? 使用 MdSymbsBase::BlockPublish 然後直接轉送 I140?
   //       => 如果 B主機 正在補商品快照, 這時 B主機 收到的 I140 要如何處理: 尚未補到快照的商品?
   // ?-3-? 直接送出各商品異動封包?

   switch (fon9::PackBcdTo<unsigned>(i140.FunctionCode_)) {
   case 100: I140_10x_LmtRange(e, -1); break;
   case 101: I140_10x_LmtRange(e,  1); break;

   case 200: I140_200_TradingSessionSt(e); break;
   case 201: I140_201_TradingSessionSt(e); break;

   case 302: I140_30x_TradingSessionSt(e, f9fmkt_TradingSessionSt_PreOpen);         break;
   case 304: I140_30x_TradingSessionSt(e, f9fmkt_TradingSessionSt_Open);            break;
   case 305: I140_30x_TradingSessionSt(e, f9fmkt_TradingSessionSt_NonCancelPeriod); break;
   case 306: I140_30x_TradingSessionSt(e, f9fmkt_TradingSessionSt_Closed);          break;

   case 400: case 403:  I140_40x_DynBand(e, f9fmkt_DynBandSt_Suspended); break;
   case 401: case 404:  I140_40x_DynBand(e, f9fmkt_DynBandSt_Resumed);   break;
   case 402: case 405:  I140_40r_DynRange(e, f9fmkt_DynBandSt_Ranged);   break;
   }
}

} // namespaces
