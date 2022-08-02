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
inline ValueT TmpGetValueT(const ByteAry<sizeof(ValueT)>& tmp) {
   return fon9::GetBigEndian<ValueT>(&tmp);
}
template <size_t kNBytes>
inline fon9::UIntTypeSelector_t<kNBytes> TmpGetValueU(const ByteAry<kNBytes>& tmp) {
   return fon9::GetBigEndian<fon9::UIntTypeSelector_t<kNBytes>>(&tmp);
}
template <size_t kNBytes>
inline fon9::SIntTypeSelector_t<kNBytes> TmpGetValueS(const ByteAry<kNBytes>& tmp) {
   return fon9::GetBigEndian<fon9::SIntTypeSelector_t<kNBytes>>(&tmp);
}

//--------------------------------------------------------------------------//

using TmpMsgLength_t = uint16_t;
using TmpMsgLength = ByteAry<sizeof(TmpMsgLength_t)>;

using TmpMsgSeqNum_t = uint32_t;
using TmpMsgSeqNum = ByteAry<sizeof(TmpMsgSeqNum_t)>;

using TmpStatusCode = uint8_t;
using TmpCheckSum = uint8_t;

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
   fon9::TimeStamp ToTimeStamp() const {
      return fon9::TimeStamp{fon9::TimeStamp::Make<3>(
         TmpGetValueU(this->Epoch_) * 1000ull
         + TmpGetValueU(this->Ms_))};
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
   TmpFcmId       SessionFcmId_;
   TmpSessionId   SessionId_;

   TmpMsgLength_t GetMsgLength() const {
      return TmpGetValueU(this->MsgLength_);
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

inline void TmpHeaderClearSt(TmpHeaderSt* pktmp) {
   pktmp->StatusCode_ = 0;
}
inline void TmpHeaderClearSt(void*) {
}

template <class TmpPk>
inline void TmpInitializeSkipSeqNum(TmpPk& pktmp, TmpMessageType msgType) {
   TmpPutValue(pktmp.MsgLength_, static_cast<TmpMsgLength_t>(sizeof(pktmp)
                                                             - sizeof(pktmp.MsgLength_)
                                                             - sizeof(pktmp.CheckSum_)));
   pktmp.MessageType_ = msgType;
   TmpHeaderClearSt(&pktmp);
}
template <class TmpPk>
inline void TmpInitializeWithSeqNum(TmpPk& pktmp, TmpMessageType msgType) {
   TmpInitializeSkipSeqNum(pktmp, msgType);
   pktmp.MsgSeqNum_.Clear();
}

//--------------------------------------------------------------------------//

using TmpSymbolSeq_t = uint16_t;
using TmpSymbolSeq = ByteAry<sizeof(TmpSymbolSeq_t)>;

enum class TmpCombOp : uint8_t {
   Single = 0,
   PriceSpread = 1,
   TimeSpread = 2,
   Straddle = 3,
   Strangle = 4,
   ConversionReversals = 5,
};

enum class TmpPriType : uint8_t {
   Market = 1,
   Limit = 2,
   /// 一定範圍市價.
   Mwp = 3,
};
inline f9fmkt_PriType TmpPriTypeTo(f9twf::TmpPriType priType) {
   switch (priType) {
   case f9twf::TmpPriType::Market:  return f9fmkt_PriType_Market;
   case f9twf::TmpPriType::Limit:   return f9fmkt_PriType_Limit;
   case f9twf::TmpPriType::Mwp:     return f9fmkt_PriType_Mwp;
   }
   return f9fmkt_PriType_Unknown;
}

enum class TmpSide : uint8_t {
   Single = 0,
   Buy = 1,
   Sell = 2,
};
inline f9fmkt_Side TmpSideTo(f9twf::TmpSide side) {
   switch (side) {
   case f9twf::TmpSide::Buy:  return f9fmkt_Side_Buy;
   case f9twf::TmpSide::Sell: return f9fmkt_Side_Sell;
   default:
   case f9twf::TmpSide::Single:
      return f9fmkt_Side_Unknown;
   }
}

enum class TmpTimeInForce : uint8_t {
   ROD = 0,
   IOC = 3,
   FOK = 4,
   /// 一定期間後系統自動刪除(報價).
   QuoteAutoCancel = 8,
};
inline f9fmkt_TimeInForce TmpTimeInForceTo(TmpTimeInForce tif) {
   switch (tif) {
   case TmpTimeInForce::ROD:  return f9fmkt_TimeInForce_ROD;
   case TmpTimeInForce::IOC:  return f9fmkt_TimeInForce_IOC;
   case TmpTimeInForce::FOK:  return f9fmkt_TimeInForce_FOK;
   case TmpTimeInForce::QuoteAutoCancel:  return f9fmkt_TimeInForce_QuoteAutoCancel;
   }
   return f9fmkt_TimeInForce{};
}

enum class TmpExecType : char {
   New = '0',
   Delete = '4',
   /// Qty=欲減數量.
   ChgQty = '5',
   /// 改價 & 改成交回報線路.
   ChgPriM = 'M',
   /// 改價 & 但不改成交回報線路.
   ChgPrim = 'm',
   Query = 'I',
   /// 新增併成交回報
   FilledNew = '6',
   Filled = 'F',
};

using TmpPosEff = ExgPosEff;

using TmpPrice_t = int32_t;
using TmpPrice = ByteAry<sizeof(TmpPrice_t)>;

using TmpAmt_t = int64_t;
using TmpAmt = ByteAry<sizeof(TmpAmt_t)>;

using TmpQty_t = uint16_t;
using TmpQty = ByteAry<sizeof(TmpQty_t)>;

using TmpIvacNo_t = uint32_t;
using TmpIvacNo = ByteAry<sizeof(TmpIvacNo_t)>;
using TmpIvacFlag = fon9::CharAry<1>;

struct TmpSource {
   /// D:專線下單(含VPN、封閉型專屬網路); A:應用程式介面(API)下單;
   /// M:行動載具下單;  W:網站(Web Site)下單; P:個人電腦軟體下單;
   /// V:語音下單;  G:一般委託下單(書面、電話、電報等方式)
   fon9::CharAry<1>     OrderSource_;

   /// 行情資訊來源註記
   /// - 由行情資訊廠商提供者，填入該行情資訊廠商代碼(詳行情資訊廠商代碼表註);
   /// - 由期貨商自行提供行情資訊者，填入 999;
   fon9::CharAry<3>     InfoSource_;
};

//--------------------------------------------------------------------------//

struct TmpSymNum {
   TmpSymbolSeq   Pseq1_;
   TmpSymbolSeq   Pseq2_;
   TmpSide        LegSide_[2];
   TmpCombOp      CombOp_;
   char           Filler_[1];
};
struct TmpSymTextS {
   fon9::CharAry<sizeof(ExgProdIdS) * 2>   Symbol_;
};
struct TmpSymTextL {
   fon9::CharAry<sizeof(ExgProdIdL) * 2>   Symbol_;
};

template <class SymTextT>
struct TmpSymIdT {
   union {
      SymTextT    Text_;
      TmpSymNum   Num_;
   }  Sym_;

   void ClearNumFiller() {
      memset(this->Sym_.Num_.Filler_, 0,
             sizeof(*this) - sizeof(this->Sym_.Num_) + sizeof(this->Sym_.Num_.Filler_));
   }
};

using TmpSymIdS = TmpSymIdT<TmpSymTextS>;
using TmpSymIdL = TmpSymIdT<TmpSymTextL>;

enum class TmpSymbolType : uint8_t {
   ShortNum  = 1,
   ShortText = 2,
   LongNum   = 3,
   LongText  = 4,
};
constexpr bool TmpSymbolTypeIsLong(TmpSymbolType v) {
   return v >= TmpSymbolType::LongNum;
}
constexpr bool TmpSymbolTypeIsShort(TmpSymbolType v) {
   return v < TmpSymbolType::LongNum;
}
constexpr bool TmpSymbolTypeIsNum(TmpSymbolType v) {
   return (static_cast<uint8_t>(v) & 0x01);
}

/// 一般而言, 在 Tmp 封包裡面, TmpSymbolType 之後, 緊接著是 TmpSymIdS 或 TmpSymIdL;
/// 可以透過 TmpPtrAfterSym() 計算「商品Id」之後的指標位置.
constexpr void* TmpPtrAfterSym(TmpSymbolType* symType) {
   return reinterpret_cast<char*>(symType + 1)
      + (TmpSymbolTypeIsShort(*symType) ? sizeof(TmpSymIdS) : sizeof(TmpSymIdL));
}
constexpr const void* TmpPtrAfterSym(const TmpSymbolType* symType) {
   return TmpPtrAfterSym(const_cast<TmpSymbolType*>(symType));
}
template <class FrontT>
constexpr typename FrontT::BackType* TmpPtrAfterSym(FrontT* tmpFront) {
   return reinterpret_cast<typename FrontT::BackType*>(TmpPtrAfterSym(&tmpFront->SymbolType_));
}
template <class FrontT>
constexpr const typename FrontT::BackType* TmpPtrAfterSym(const FrontT* tmpFront) {
   return reinterpret_cast<const typename FrontT::BackType*>(TmpPtrAfterSym(&tmpFront->SymbolType_));
}

/// 用於「三段式: FrontT + SymIdT + BackT」的格式定義.
/// - FrontT 應為: 「TmpHeader 或 TmpHeaderSt」 + 「商品Id之前的欄位」 + SymbolType_;
/// - SymIdT = TmpSymIdS 或 TmpSymIdL;
/// - 預設 BackT = FrontT::BackType;
template <class FrontT, class SymIdT, class BackT = typename FrontT::BackType>
struct TmpTriDef : public FrontT, public SymIdT, public BackT {
};

//--------------------------------------------------------------------------//

using TmpRUsrDef = fon9::CharAry<8>;

struct TmpR03 : public TmpHeaderSt {
   TmpExecType    ExecType_;
   TmpFcmId       IvacFcmId_;
   OrdNo          OrderNo_;
   TmpOrdId       OrdId_;
   TmpRUsrDef     UserDefine_;
   TmpMsgSeqNum   RptSeq_;
   TmpSide        Side_;
   TmpCheckSum    CheckSum_;
};
static_assert(sizeof(TmpR03) == 44, "struct TmpR03; must pack?");

//--------------------------------------------------------------------------//

struct f9twf_API ExgCombSymbId {
   using Id = fon9::CharAryL<sizeof(ExgProdIdL)>;
   Id          LegId1_;
   Id          LegId2_;
   ExgCombSide CombSide_;
   TmpCombOp   CombOp_;

   bool Parse(fon9::StrView symbid);
};

/// - leg1, leg2 必須同時是長Id, 或同時是短Id;
/// - 期貨只有 TmpCombOp::TimeSpread;
/// - 您必須依照期交所規範決定 leg1, leg2 的順序, 這裡單純將 leg1 放在前方, leg2 放在後方.
f9twf_API bool FutMakeCombSymbolId(SymbolId& symb, fon9::StrView leg1, fon9::StrView leg2, TmpCombOp op);
f9twf_API bool OptMakeCombSymbolId(SymbolId& symb, fon9::StrView leg1, fon9::StrView leg2, TmpCombOp op);

/// 是否為台灣期交所期貨複式商品(短商品Id格式)?
static inline bool IsFutShortComb(const char* futShortSymbId) {
   return futShortSymbId[5] == '/';
}

/// 返回 TmpSymbolType::ShortText 或 TmpSymbolType::LongText;
inline TmpSymbolType FutCheckSymbolType(const SymbolId& symbid, bool isComb) {
   // ShortId: PPPCC(5) or PPPCC/DD(8) or PPPCC/PPWDD(11)
   // LongId:  PPPPPPSTCC(10) or PPPPPPSTCC/DD(13)
   return (symbid.length() <= (isComb ? 11 : 5))
      ? f9twf::TmpSymbolType::ShortText
      : f9twf::TmpSymbolType::LongText;
}
inline TmpSymbolType OptCheckSymbolType(const SymbolId& symbid, bool isComb) {
   // PPPAAAAACC(10) or PPPAAAAACC/DD(13) or PPPAAAAA/BBBBBCC(16) or PPPAAAAACC:BBBBBDD(18) or PPPAAAAACC/PPWDD(16)
   // PPPPPPSTpppppCC(15) or PPPPPPSTAAAAA/BBBBBCC(21) or PPPPPPSTAAAAACC/DD(18) or PPPPPPSTAAAAACC:BBBBBDD(23)
   if (fon9_LIKELY(!isComb))
      return (symbid.length() <= 10
              ? f9twf::TmpSymbolType::ShortText
              : f9twf::TmpSymbolType::LongText);
   if (symbid.length() < 18)
      return f9twf::TmpSymbolType::ShortText;
   if (symbid.length() > 18)
      return f9twf::TmpSymbolType::LongText;
   return (fon9::isalnum(fon9::unsigned_cast(*(symbid.begin() + 15)))
           ? f9twf::TmpSymbolType::ShortText
           : f9twf::TmpSymbolType::LongText);
}

} // namespaces
#endif//__f9twf_ExgTmpTypes_hpp__
