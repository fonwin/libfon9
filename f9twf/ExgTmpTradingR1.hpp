// \file f9twf/ExgTmpTradingR1.hpp
//
// TMP封包: 一般下單R01/R31; 一般下單回報R02/R32; 短格式回報R22;
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTmpTradingR1_hpp__
#define __f9twf_ExgTmpTradingR1_hpp__
#include "f9twf/ExgTmpTypes.hpp"

namespace f9twf {

struct TmpR1BfSym {
   TmpExecType    ExecType_;
   TmpFcmId       CmId_;
   TmpFcmId       IvacFcmId_;
   OrdNo          OrderNo_;
   TmpOrdId       OrdId_;
   TmpRUsrDef     UserDefine_;
   TmpSymbolType  SymbolType_;
};

/// R01/R31/R02/R32 在商品欄位之後的資料.
struct TmpR1AfSym {
   TmpPrice       Price_;
   TmpQty         Qty_;
   TmpIvacNo      IvacNo_;
   TmpIvacFlag    IvacFlag_;
   TmpSide        Side_;
   TmpPriType     PriType_;
   TmpTimeInForce TimeInForce_;
   TmpPosEff      PosEff_;
};

//--------------------------------------------------------------------------//

struct TmpR1Front : public TmpHeader, public TmpR1BfSym {
   struct BackType : public TmpR1AfSym {
      TmpSource   Source_;
      TmpCheckSum CheckSum_;
   };
};
using TmpR1Back = TmpR1Front::BackType;
using TmpR01 = TmpTriDef<TmpR1Front, TmpSymIdS>;
using TmpR31 = TmpTriDef<TmpR1Front, TmpSymIdL>;

static_assert(sizeof(TmpR01) ==  80, "struct TmpR01; must pack?");
static_assert(sizeof(TmpR31) == 100, "struct TmpR31; must pack?");

//--------------------------------------------------------------------------//

struct TmpR2Front : public TmpHeaderSt, public TmpR1BfSym {
   struct BackType : public TmpR1AfSym {
      TmpPrice       LastPx_;
      TmpQty         LastQty_;
      TmpAmt         PxSubTotal_;
      TmpQty         CumQty_;
      TmpQty         LeavesQty_;
      TmpQty         BeforeQty_;
      TmpSide        LegSide_[2];
      TmpPrice       LegPx_[2];
      TmpQty         LegQty_[2];
      TmpMsgTime     OrgTransTime_;
      TmpMsgTime     TransactTime_;

      /// 期貨商:4; 結算會員委託回報:8; 結算會員成交回報:9;
      uint8_t        TargetId_;
      /// target_id=4 則為系統唯一序號(由期交所編製);
      /// target_id=8,9 則為回報序號(cm_sub_seq);
      TmpMsgSeqNum   UniqId_;
      /// target_id=4 時為 session 流水號(session_seq);
      /// target_id=8,9 為結算會員回報序號(cm_seq);
      TmpMsgSeqNum   RptSeq_;
      /// 新單資料來源: 0:保留; 1:TMP; 2:FIX;
      uint8_t        ProtocolType_;

      TmpCheckSum    CheckSum_;
   };
};
using TmpR2Back = TmpR2Front::BackType;
using TmpR02 = TmpTriDef<TmpR2Front, TmpSymIdS>;
using TmpR32 = TmpTriDef<TmpR2Front, TmpSymIdL>;

static_assert(sizeof(TmpR02) == 133, "struct TmpR02; must pack?");
static_assert(sizeof(TmpR32) == 153, "struct TmpR32; must pack?");

//--------------------------------------------------------------------------//

/// 當 L40 中 ap_code 填 6 時，傳送 R22 格式之委託與成交回報。
struct TmpR22 : public TmpHeaderSt {
   TmpExecType    ExecType_;
   TmpFcmId       IvacFcmId_;
   OrdNo          OrderNo_;
   TmpOrdId       OrdId_;
   TmpRUsrDef     UserDefine_;
   TmpSide        Side_;
   TmpPosEff      PosEff_;
   TmpQty         LeavesQty_;
   TmpQty         BeforeQty_;
   TmpPrice       LegPx_[2];
   TmpQty         LegQty_[2];
   TmpMsgTime     TransactTime_;
   TmpMsgSeqNum   UniqId_;
   TmpMsgSeqNum   RptSeq_;
   uint8_t        ProtocolType_;
   TmpPrice       Price_;
   TmpCheckSum    CheckSum_;
};
static_assert(sizeof(TmpR22) == 76, "struct TmpR22; must pack?");

} // namespaces
#endif//__f9twf_ExgTmpTradingR1_hpp__
