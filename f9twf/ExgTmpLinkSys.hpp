// \file f9twf/ExgTmpLinkSys.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTmpLinkSys_hpp__
#define __f9twf_ExgTmpLinkSys_hpp__
#include "f9twf/ExgTmpTypes.hpp"

namespace f9twf {

struct TmpL10 : public TmpHeaderSt {
   TmpMsgSeqNum   StartInBoundNum_;
   TmpCheckSum    CheckSum_;

   void Initialize() {
      TmpInitializeWithSeqNum(*this, TmpMessageType_L(10));
      this->StartInBoundNum_.Clear();
   }
};

struct TmpCfmBase : public TmpHeaderSt {
   TmpCheckSum    CheckSum_;

   void Initialize(TmpMessageType msgType) {
      TmpInitializeWithSeqNum(*this, msgType);
   }
};
using TmpL20 = TmpCfmBase;

struct TmpL30 : public TmpHeaderSt {
   ByteAry<2>     AppendNo_;
   /// 上次結尾訊息編號.
   /// 期交所送給期貨商之訊息末筆有效回報訊息序號.
   /// (結算會員委託成交回報線路填 0、TMPDC Session 填 0)
   TmpMsgSeqNum   EndOutBoundNum_;
   ExgSystemType  SystemType_;
   uint8_t        EncryptMethod_;
   TmpCheckSum    CheckSum_;

   void Initialize(ExgSystemType sysType, TmpMsgSeqNum_t endOutBoundNum) {
      TmpInitializeWithSeqNum(*this, TmpMessageType_L(30));
      TmpPutValue(this->EndOutBoundNum_, endOutBoundNum);
      this->SystemType_ = sysType;
      this->EncryptMethod_ = 0;
   }
};

struct TmpL40 : public TmpHeaderSt {
   ByteAry<2>     AppendNo_;
   TmpFcmId       FcmId_;
   TmpSessionId   SessionId_;
   ExgSystemType  SystemType_;
   TmpApCode      ApCode_;
   uint8_t        KeyValue_;
   /// 請求期交所從該值所代表之後一序號開始補送訊息.
   /// 即期貨商提出 RequestStartSeq_ + 1 至 L30.EndOutBoundNum_ 的訊息請求補送.
   TmpMsgSeqNum   RequestStartSeq_;
   uint8_t        CancelOrderSec_;
   TmpCheckSum    CheckSum_;

   /// 除了 ApCode_ 及 KeyValue_;
   void Initialize(const TmpL30& src, TmpMsgSeqNum_t requestStartSeq) {
      TmpInitializeWithSeqNum(*this, TmpMessageType_L(40));
      this->AppendNo_ = src.AppendNo_;
      this->FcmId_ = src.SessionFcmId_;
      this->SessionId_ = src.SessionId_;
      this->SystemType_ = src.SystemType_;
      this->CancelOrderSec_ = 0;
      TmpPutValue(this->RequestStartSeq_, requestStartSeq);
   }
};

struct TmpL41 : public TmpHeaderSt {
   uint8_t     IsEof_;
   ByteAry<4>  FileSize_;
   /// [MsgLength_ – 15 - 6];
   /// Data_[] 內含 RequestStartSeq_ + 1 至 EndOutBoundNum_ 的
   /// 所有交易子系統之完整訊息(訊息序號為 0 除外).
   /// 若 L40 中 ApCode_ 為 4: 則委託/成交回報採用 R02/R32 格式.
   /// 若 L40 中 ApCode_ 為 6: 則委託/成交回報採用 R22 格式.
   uint8_t     Data_[1];
};
using TmpL42 = TmpCfmBase;

struct TmpL50 : public TmpHeaderSt {
   uint8_t     HeartBtInt_;
   ByteAry<2>  MaxFlowCtrlCnt_;
   TmpCheckSum CheckSum_;
};

using TmpL60 = TmpCfmBase;

using TmpR04 = TmpCfmBase;
using TmpR05 = TmpCfmBase;

} // namespaces
#endif//__f9twf_ExgTmpLinkSys_hpp__
