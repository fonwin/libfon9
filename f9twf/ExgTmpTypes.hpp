// \file f9twf/ExgTmpTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTmpTypes_hpp__
#define __f9twf_ExgTmpTypes_hpp__
#include "f9twf/ExgTypes.hpp"
#include "fon9/Endian.hpp"
#include "fon9/TimeStamp.hpp"

namespace f9twf {

using byte = fon9::byte;

template <size_t N>
using ByteAry = fon9::CharAryF<N, byte>;

template <typename ValueT>
inline void TmpPutValue(ByteAry<sizeof(ValueT)>& tmp, ValueT val) {
   fon9::PutBigEndian(&tmp, val);
}

template <typename ValueT>
inline ValueT TmpGetValue(const ByteAry<sizeof(ValueT)>& tmp) {
   return fon9::GetBigEndian<ValueT>(&tmp);
}

//--------------------------------------------------------------------------//

using TmpMsgLength_t = uint16_t;
using TmpMsgLength = ByteAry<sizeof(TmpMsgLength_t)>;

using TmpMsgSeqNum_t = uint32_t;
using TmpMsgSeqNum = ByteAry<sizeof(TmpMsgSeqNum_t)>;

using TmpStatusCode = uint8_t;
using TmpCheckSum   = uint8_t;

/// 與 OrdNo 不同, TmpOrdId 可視為自訂序號.
/// 可用來對應券商內部的資料.
using TmpOrdId_t = uint32_t;
using TmpOrdId = ByteAry<sizeof(TmpOrdId_t)>;

/// TCP 連線編號.
using TmpSessionId_t = SessionId;
using TmpSessionId = ByteAry<sizeof(TmpSessionId_t)>;

/// 期貨商編號, 參考 P06;
using TmpFcmId_t = FcmId;
using TmpFcmId = ByteAry<sizeof(TmpFcmId_t)>;

struct TmpMsgTime {
   ByteAry<4>  Epoch_;
   ByteAry<2>  Ms_;

   void AssignFrom(fon9::TimeStamp now) {
      TmpPutValue(this->Epoch_, static_cast<uint32_t>(now.GetIntPart()));
      TmpPutValue(this->Ms_, static_cast<uint16_t>(now.GetDecPart() / (now.Divisor / 1000)));
   }
};

enum class TmpMessageType : uint8_t {
   LinkSys = 0,
   ApSys = 100,
};
/// TmpMessageType_L(10) = L10;
constexpr TmpMessageType TmpMessageType_L(uint8_t n) {
   return static_cast<TmpMessageType>(n + fon9::cast_to_underlying(TmpMessageType::LinkSys));
}
/// TmpMessageType_R(01) = R01;
constexpr TmpMessageType TmpMessageType_R(uint8_t n) {
   return static_cast<TmpMessageType>(n + fon9::cast_to_underlying(TmpMessageType::ApSys));
}

struct TmpHeader {
   TmpMsgLength   MsgLength_;
   TmpMsgSeqNum   MsgSeqNum_;
   TmpMsgTime     MsgTime_;
   TmpMessageType MessageType_;
   TmpFcmId       FcmId_;
   TmpSessionId   SessionId_;

   TmpMsgLength_t GetMsgLength() const {
      return TmpGetValue<TmpMsgLength_t>(this->MsgLength_);
   }
   size_t GetPacketSize() const {
      return this->GetMsgLength() + sizeof(this->MsgLength_) + sizeof(TmpCheckSum);
   }
};
static_assert(sizeof(TmpHeader) == 17, "struct TmpHeader must pack?");

struct TmpHeaderSt : public TmpHeader {
   TmpStatusCode  StatusCode_;
};
static_assert(sizeof(TmpHeaderSt) == 18, "struct TmpHeaderSt must pack?");

/// 計算封包的 CheckSum.
/// pksz = 包含完整 TmpHeader 及 CheckSum 的封包大小.
f9twf_API TmpCheckSum TmpCalcCheckSum(const TmpHeader& pktmp, size_t pksz);

//--------------------------------------------------------------------------//

enum class TmpApCode : uint8_t {
   /// 期貨商委託/成交.
   Trading = 4,
   /// 期貨商短格式委託/成交回報.
   TradingShortFormat = 6,
   /// 結算會員委託/成交回報.
   ReportCm = 8,
   /// TMPDC Session.
   TmpDc = 7,
};
inline char TmpApCodeToAlpha(TmpApCode apCode) {
   return static_cast<char>(static_cast<uint8_t>(apCode) + '0');
}

template <class TmpPk>
inline void TmpInitializeNoSeqNum(TmpPk& pktmp, TmpMessageType msgType) {
   TmpPutValue(pktmp.MsgLength_, static_cast<TmpMsgLength_t>(sizeof(pktmp)
                                                             - sizeof(pktmp.MsgLength_)
                                                             - sizeof(pktmp.CheckSum_)));
   pktmp.MessageType_ = msgType;
   pktmp.StatusCode_ = 0;
}
template <class TmpPk>
inline void TmpInitializeWithSeqNum(TmpPk& pktmp, TmpMessageType msgType) {
   TmpInitializeNoSeqNum(pktmp, msgType);
   pktmp.MsgSeqNum_.Clear();
}

//--------------------------------------------------------------------------//

using TmpSymbolSeq = ByteAry<2>;

enum class TmpCombOp : uint8_t {
   Single = 0,
   PriceSpread = 1,
   TimeSpread = 2,
   Straddle = 3,
   Strangle = 4,
   ConversionReversals = 5,
};

enum class TmpPriType : char {
   Market = 1,
   Limit = 2,
   /// 一定範圍市價.
   Wmp = 3,
};

enum class TmpSide : char {
   Buy = 1,
   Sell = 2,
};

enum class TmpTimeInForce : char {
   ROD = 0,
   IOC = 3,
   FOK = 4,
};

enum class TmpPosEff : char {
   Open = 'O',
   Close = 'C',
   DayTrade = 'D',
   /// 報價.
   Quote = '9',
   /// 選擇權新倉含指定部位沖銷:A(Options Only)
   /// Open & specified position offsets.
   OptOpenAndClear = 'A',
   /// 代為沖銷.
   ForceClear = '7',
};

enum class ExecType : char {
   New = '0',
   Delete = '4',
   /// Qty=欲減數量.
   ChgQty = '5',
   /// 改價 & 改成交回報線路.
   ChgPriM = 'M',
   /// 改價 & 但不改成交回報線路.
   ChgPrim = 'm',
   Query = 'I',
};

} // namespaces
#endif//__f9twf_ExgTmpTypes_hpp__
