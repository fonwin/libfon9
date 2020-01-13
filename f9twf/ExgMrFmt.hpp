// \file f9twf/ExgMrFmt.hpp
//
// ExgMrFmt = Exg(TaiFex) Market Recover format.
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMrFmt_hpp__
#define __f9twf_ExgMrFmt_hpp__
#include "f9twf/ExgTmpTypes.hpp"

namespace f9twf {

using ExgMrMsgSize_t = uint16_t;
using ExgMrMsgSize = ByteAry<sizeof(ExgMrMsgSize_t)>;

using ExgMrMsgType_t = uint16_t;
using ExgMrMsgType = ByteAry<sizeof(ExgMrMsgType_t)>;

using ExgMrMsgSeqNum_t = uint32_t;
using ExgMrMsgSeqNum = ByteAry<sizeof(ExgMrMsgSeqNum_t)>;

struct ExgMrMsgTime {
   ByteAry<4>  Epoch_;
   ByteAry<4>  Nano_;

   void AssignFrom(fon9::TimeStamp now) {
      TmpPutValue(this->Epoch_, static_cast<uint32_t>(now.GetIntPart()));
      TmpPutValue(this->Nano_, static_cast<uint32_t>(now.ShiftUnit<9>() % fon9::DecDivisor<uint64_t, 9>::Divisor));
   }
   fon9::TimeStamp ToTimeStamp() const {
      return fon9::TimeStamp{fon9::TimeStamp::Make<9>(
         TmpGetValueU(this->Epoch_) * fon9::DecDivisor<uint64_t, 9>::Divisor
         + TmpGetValueU(this->Nano_))};
   }
};

struct ExgMrMsgHeader {
   ExgMrMsgSize   MsgSize_;
   ExgMrMsgType   MsgType_;
   ExgMrMsgSeqNum MsgSeqNum_;
   ExgMrMsgTime   MsgTime_;

   ExgMrMsgSize_t GetMsgSize() const {
      return TmpGetValueU(this->MsgSize_);
   }
   unsigned GetPacketSize() const {
      return static_cast<unsigned>(this->GetMsgSize() + sizeof(this->MsgSize_) + sizeof(TmpCheckSum));
   }
   void InitHeader(size_t pksz, ExgMrMsgType_t msgType, ExgMrMsgSeqNum_t msgSeqNum) {
      TmpPutValue(this->MsgSize_, static_cast<ExgMrMsgSize_t>(pksz - (sizeof(this->MsgSize_) + sizeof(TmpCheckSum))));
      TmpPutValue(this->MsgType_, msgType);
      TmpPutValue(this->MsgSeqNum_, msgSeqNum);
      this->MsgTime_.AssignFrom(fon9::UtcNow());
   }
};
static_assert(sizeof(ExgMrMsgHeader) == 16, "struct ExgMrMsgHeader must pack?");

//--------------------------------------------------------------------------//
using ExgMrCheckSum = uint8_t;

struct ExgMrMsgFooter {
   ExgMrCheckSum CheckSum_;
};

static inline ExgMrCheckSum ExgMrCalcCheckSum(const ExgMrMsgHeader& pk, unsigned pksz) {
   assert(pk.GetPacketSize() == pksz);
   const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&pk);
   ExgMrCheckSum  chk = *ptr;
   for (pksz -= 2; pksz > 0; --pksz) {
      fon9_GCC_WARN_DISABLE("-Wconversion");
      chk += *++ptr;
      fon9_GCC_WARN_POP;
   }
   return chk;
}
static inline const ExgMrMsgFooter& GetExgMrMsgFooter(const ExgMrMsgHeader& pk, unsigned pksz) {
   assert(pk.GetPacketSize() == pksz);
   return *reinterpret_cast<const ExgMrMsgFooter*>(
      reinterpret_cast<const char*>(&pk) + pksz - sizeof(ExgMrMsgFooter));
}

//--------------------------------------------------------------------------//
using ExgMrSessionId_t = uint16_t;
using ExgMrSessionId = ByteAry<sizeof(ExgMrSessionId_t)>;

struct ExgMrLoginReqBody {
   /// 登錄運算子 輔助密碼計算使用，應大於 1。
   ByteAry<sizeof(uint16_t)>  MultiplicationOperator_;
   /// 登錄檢查碼 (MultiplicationOperator * 申請表單中自行設定之密碼)，取千與百兩位數字.
   uint8_t                    CheckCode_;
   ExgMrSessionId             SessionId_;
};
/// 資訊接收端用此訊息向期交所行情回補伺服器請求登錄，每次登錄之後 MsgSeqNum 將會重新計算。
struct ExgMrLoginReq020 : public ExgMrMsgHeader, public ExgMrLoginReqBody, public ExgMrMsgFooter {
   void Final(ExgMrMsgSeqNum_t msgSeqNum) {
      this->InitHeader(sizeof(*this), 20, msgSeqNum);
      this->CheckSum_ = ExgMrCalcCheckSum(*this, sizeof(*this));
   }
};
//--------------------------------------------------------------------------//
using ExgMrChannelId_t = uint16_t;
using ExgMrChannelId = ByteAry<sizeof(ExgMrChannelId_t)>;

struct ExgMrLoginCfmBody {
   ExgMrChannelId   ChannelId_;
};
/// 期交所用此訊息回覆資訊接收端登錄結果，如有多個 ChannelID，則 030 會有多筆。
struct ExgMrLoginCfm030 : public ExgMrMsgHeader, public ExgMrLoginCfmBody, public ExgMrMsgFooter {
};

//--------------------------------------------------------------------------//
struct ExgMrNoBody : public ExgMrMsgHeader, public ExgMrMsgFooter {
   void Final(ExgMrMsgType_t msgType, ExgMrMsgSeqNum_t msgSeqNum) {
      this->InitHeader(sizeof(*this), msgType, msgSeqNum);
      this->CheckSum_ = ExgMrCalcCheckSum(*this, sizeof(*this));
   }
};

/// 回補服務開始(050): 期交所至資訊接收端.
using ExgMrStart050 = ExgMrNoBody;

/// 確認連線訊息(104): Heartbeat. 期交所至資訊接收端.
/// 期交所系統若於一定時間未收到該 Session 新訊息，會主動發出 Heartbeat 訊息，
/// Client 端需於一定時間內回應，否則將中斷該 Session 連線.
using ExgMrHeartbeat104 = ExgMrNoBody;
/// 確認連線訊息(105): Heartbeat. 資訊接收端至期交所.
using ExgMrHeartbeat105 = ExgMrNoBody;

//--------------------------------------------------------------------------//
using ExgMrSeqNo_t = uint32_t;
using ExgMrSeqNo = ByteAry<sizeof(ExgMrSeqNo_t)>;

using ExgMrRecoverNum_t = uint16_t;
using ExgMrRecoverNum = ByteAry<sizeof(ExgMrRecoverNum_t)>;

struct ExgMrRecoverReqBody {
   /// 對應 Multicast 群組定義，CHANNEL-ID.
   ExgMrChannelId   ChannelId_;
   /// 回補起始訊息序號.
   ExgMrSeqNo       BeginSeqNo_;
   /// 小於或等於 1 時，則是代表回補起始訊息那一筆。
   /// 值若大於限制上限則為限制上限。
   ExgMrRecoverNum  RecoverNum_;
};
/// 請求行情資料回補訊息(101).
struct ExgMrRecoverReq101 : public ExgMrMsgHeader, public ExgMrRecoverReqBody, public ExgMrMsgFooter {
   void Final(ExgMrMsgSeqNum_t msgSeqNum) {
      this->InitHeader(sizeof(*this), 101, msgSeqNum);
      this->CheckSum_ = ExgMrCalcCheckSum(*this, sizeof(*this));
   }
};

//--------------------------------------------------------------------------//
using ExgMrStatusCode = uint8_t;

struct ExgMrRecoverStBody {
   ExgMrChannelId   ChannelId_;
   ExgMrStatusCode  StatusCode_;
   ExgMrSeqNo       BeginSeqNo_;
   ExgMrRecoverNum  RecoverNum_;
};
/// 期交所傳送請求回補狀態回覆至資訊接收端。如果該回補要求被接受，後續會發送原始行情電文。
struct ExgMrRecoverSt102 : public ExgMrMsgHeader, public ExgMrRecoverStBody, public ExgMrMsgFooter {
};

//--------------------------------------------------------------------------//
struct ExgMrErrorBody {
   ExgMrStatusCode  StatusCode_;
};
/// 若需要回傳錯誤碼並要求 Client 重新連線，
/// 則期交所端會回覆 010 通知訊息以及錯誤代碼，逕行斷線.
struct ExgMrError010 : public ExgMrMsgHeader, public ExgMrErrorBody, public ExgMrMsgFooter {
};

//--------------------------------------------------------------------------//
// 目前 [臺灣期貨交易所 逐筆行情資訊傳輸作業手冊 V1.0.0] 的錯誤狀態碼說明:
//
// 編號 | 訊息說明                    | 資訊接受端應處理事項
// -----|----------------------------|----------------------------------------
//   0  | 訊息接收成功                | 輸入下一筆訊息
//   1  | 登錄檢查碼運算錯誤          | 請檢查 MultiplicationOperator 與 CheckCode 欄位
//   2  | 重複登錄                   | 結束此次連線       
//   3  | BeginSeqNo 不在可回補範圍內 | 更正回補起始訊息序號，重新發送
//   4  | SessionID 錯誤             | 檢查並更正 Session ID
//   5  | 備援尚未開放回補            | 請由主服務主機回補資料
//   6  | ChannelID 錯誤             | 檢查並更正 ChannelID
//   7  | 連線 IP 或連線埠錯誤        | 請確認是否使用申請之連線 IP 及連線埠連線
//   8  | 檢核碼錯誤                 | 檢查並更正 CheckSum
//   9  | 服務要求次數已滿            | 請檢查是否達到服務要求次數上限。
//  10  | Heartbeat 逾時             | 連線過久沒有回應，請重新登入
//  11  | RecoverNum 錯誤            | 更正回補筆數，重新發送
//  98  | 傳送訊息格式錯誤            | 檢查並更正電文內容
//--------------------------------------------------------------------------//

} // namespaces
#endif//__f9twf_ExgMrFmt_hpp__
