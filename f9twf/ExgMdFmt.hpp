// \file f9twf/ExgMdFmt.hpp
//
// 台灣期交所行情格式: 框架定義.
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmt_hpp__
#define __f9twf_ExgMdFmt_hpp__
#include "f9twf/Config.h"
#include "fon9/CharAry.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/fmkt/TwExgMdTime.hpp"

namespace f9twf {
fon9_PACK(1);

using ExgMdHHMMSSu6 = fon9::fmkt::TwExgMdTimeHHMMSSu6;
using ExgMdProdId20 = fon9::CharAry<20>;
using ExgMdProdId10 = fon9::CharAry<10>;
/// 逐筆行情: 商品行情訊息流水序號.
using ExgMcProdMsgSeq = fon9::PackBcd<10>;

using ExgMdPrice = fon9::PackBcd<9>;
inline void ExgMdPriceTo(fon9::fmkt::Pri& dst, const ExgMdPrice& src, uint32_t origDiv) {
   dst.SetOrigValue(fon9::signed_cast(fon9::PackBcdTo<uint64_t>(src)));
   if (origDiv)
      dst *= origDiv;
}
inline void ExgMdPriceFrom(ExgMdPrice& dst, fon9::fmkt::Pri src, uint32_t origDiv) {
   if (origDiv)
      src /= origDiv;
   fon9::ToPackBcd(dst, fon9::unsigned_cast(src.GetOrigValue()));
}
/// 價格(含正負號).
/// - 小數位數請參考 fon9::fmkt::Symb::PriceOrigDiv_;
///   - 來自 I010 訊息的 DECIMAL-LOCATOR 價格欄位小數位數。
struct ExgMdSPrice {
   /// 價格正負號.
   char        Sign_;
   ExgMdPrice  Value_;
   int64_t GetValue() const {
      auto v = static_cast<int64_t>(fon9::PackBcdTo<uint64_t>(this->Value_));
      return this->Sign_ == '-' ? (-v) : v;
   }
   void AssignTo(fon9::fmkt::Pri& dst, uint32_t origDiv) const {
      ExgMdPriceTo(dst, this->Value_, origDiv);
      if (this->Sign_ == '-')
         dst.SetOrigValue(-dst.GetOrigValue());
   }
   void AssignFrom(fon9::fmkt::Pri src, uint32_t origDiv) {
      if (fon9_LIKELY(src.GetOrigValue() >= 0))
         this->Sign_ = '0';
      else {
         this->Sign_ = '-';
         src.SetOrigValue(-src.GetOrigValue());
      }
      ExgMdPriceFrom(this->Value_, src, origDiv);
   }
};

/// 台灣期交所行情的封包框架:
/// - Head
///   - Head0
///   - 序號:
///     - 間隔行情: HeadInfoSeq
///     - 逐筆行情: HeadChannelSeq
///   - HeadVerLen
/// - Body
/// - Tail
///   - 檢查碼 1 byte; (Esc...檢查碼)之間(不含Esc、檢查碼)的 XOR 值.
///   - 結束碼 2 bytes; [0]=0x0d; [1]=0x0a;
struct ExgMdHead0 {
   char           Esc_;
   char           TransmissionCode_;
   char           MessageKind_;
   ExgMdHHMMSSu6  InformationTime_;
};
struct ExgMiHeadInfoSeq {
   fon9::PackBcd<8>  InformationSeq_;
};
struct ExgMcHeadChannelSeq {
   fon9::PackBcd<4>  ChannelId_;
   fon9::PackBcd<10> ChannelSeq_;

   uint16_t GetChannelId() const {
      return fon9::PackBcdTo<uint16_t>(this->ChannelId_);
   }
   uint64_t GetChannelSeq() const {
      return fon9::PackBcdTo<uint64_t>(this->ChannelSeq_);
   }
};
struct ExgMdHeadVerLen {
   fon9::PackBcd<2>  VersionNo_;
   fon9::PackBcd<4>  BodyLength_;

   unsigned GetBodyLength() const { return fon9::PackBcdTo<unsigned>(this->BodyLength_); }
   uint8_t  GetVersionNo()  const { return fon9::PackBcdTo<uint8_t>(this->VersionNo_);   }
};
enum : size_t {
   kExgMdMaxPacketSize = fon9::DecDivisor<size_t, sizeof(ExgMdHeadVerLen::BodyLength_) * 2>::Divisor + 64,
};

struct ExgMdTail {
   fon9::byte  CheckSum_;
   char        CrLf_[2];
};
//--------------------------------------------------------------------------//
/// 逐筆行情資訊 Head.
struct ExgMcHead : public ExgMdHead0
                 , public ExgMcHeadChannelSeq
                 , public ExgMdHeadVerLen {
};
struct ExgMcNoBody : public ExgMcHead
                   , public ExgMdTail {
};
static_assert(sizeof(ExgMcNoBody) == 22, "ExgMcNoBody 沒有 pack?");
inline unsigned GetPkSize_ExgMc(const ExgMcHead& pk) {
   return static_cast<unsigned>(pk.GetBodyLength() + sizeof(ExgMcNoBody));
}

/// 逐筆行情資訊: I001 心跳訊息 (TransmissionCode_='0',MessageKind_='1').
/// 若傳輸群組在一定時間間隔無訊息傳送時，則會傳送心跳訊息，
/// 每一個傳輸群組各自傳送該群組之心跳訊息，
/// 且在行情訊息共用檔頭中的 ChannelSeq_ 欄位會為該傳輸群組之末筆序號訊息。
using ExgMcI001 = ExgMcNoBody;
/// 逐筆行情資訊: I002 序號重置訊息 (TransmissionCode_='0',MessageKind_='2')
/// 當期交所系統發生異常，發生同地或異地系統重啟時，將會發送序號重置訊息，
/// 資訊用戶收到此訊息後，若該 CHANNEL 屬即時行情群組則須清空各商品委託簿，
/// 並重置該傳輸群組之群組序號，同時重置各商品行情訊息流水序號，
/// 並等待接收委託簿各項資訊。
using ExgMcI002 = ExgMcNoBody;
//--------------------------------------------------------------------------//
/// 間隔行情資訊 Head.
struct ExgMiHead : public ExgMdHead0
                 , public ExgMiHeadInfoSeq
                 , public ExgMdHeadVerLen {
};
struct ExgMiNoBody : public ExgMiHead
                   , public ExgMdTail {
};
static_assert(sizeof(ExgMiNoBody) == 19, "ExgMiNoBody 沒有 pack?");
inline unsigned GetPkSize_ExgMi(const ExgMiHead& pk) {
   return static_cast<unsigned>(pk.GetBodyLength() + sizeof(ExgMiNoBody));
}

/// 間隔行情資訊: I000 心跳訊息 (TransmissionCode_='0',MessageKind_='0')
/// 期交所週期性地對每一個 multicast group 送出心跳訊息，約每 30 秒傳送一次。
using ExgMiI000 = ExgMiNoBody;

fon9_PACK_POP;

//--------------------------------------------------------------------------//
constexpr uint8_t AlNumToIdx(char ch) {
   return fon9::unsigned_cast(ch) <= '9' ? static_cast<uint8_t>(ch - '0')
      : fon9::unsigned_cast(ch) <= 'Z' ? static_cast<uint8_t>(ch - 'A' + 10)
      : fon9::unsigned_cast(ch) <= 'z' ? static_cast<uint8_t>(ch - 'a' + 36)
      : static_cast<uint8_t>(0xff);
}
static_assert(AlNumToIdx('Z') + 1 == AlNumToIdx('a'), "");

enum {
   kExgMdMaxTx = AlNumToIdx('9'),
   kExgMdMaxMg = AlNumToIdx('E'),
   kExgMdMaxVer = 10,
};

/// 透過 [tx][mg][ver] 來尋找 handler.
template <class HandlerT>
struct ExgMdMessageDispatcher {
   HandlerT HandlerMap_[kExgMdMaxTx + 1][kExgMdMaxMg + 1][kExgMdMaxVer + 1];

   ExgMdMessageDispatcher() {
      memset(this->HandlerMap_, 0, sizeof(this->HandlerMap_));
   }

   void Reg(char tx, char mg, uint8_t ver, HandlerT handler) {
      assert(AlNumToIdx(tx) <= kExgMdMaxTx && AlNumToIdx(mg) <= kExgMdMaxMg && ver <= kExgMdMaxVer);
      HandlerMap_[AlNumToIdx(tx)][AlNumToIdx(mg)][ver] = std::move(handler);
   }

   template <class HeadT>
   HandlerT& Get(const HeadT& pk) {
      assert(AlNumToIdx(pk.TransmissionCode_) <= kExgMdMaxTx
             && AlNumToIdx(pk.MessageKind_) <= kExgMdMaxMg
             && fon9::PackBcdTo<uint8_t>(pk.VersionNo_) <= kExgMdMaxVer);
      return this->HandlerMap_
         [AlNumToIdx(pk.TransmissionCode_)]
         [AlNumToIdx(pk.MessageKind_)]
         [fon9::PackBcdTo<uint8_t>(pk.VersionNo_)];
   }
   template <class HeadT>
   const HandlerT& Get(const HeadT& pk) const {
      return const_cast<ExgMdMessageDispatcher*>(this)->Get(pk);
   }
};

/// 透過 [tx][mg][ver] 來尋找 parser.
template <class HeadT, class ParserArgsT, typename FnExgMdParserT = void (*)(const HeadT& pk, unsigned pksz, ParserArgsT& dst)>
struct ExgMdParserMap : public ExgMdMessageDispatcher<FnExgMdParserT> {
   bool Call(const HeadT& pk, unsigned pksz, ParserArgsT& dst) const {
      if (auto fn = this->Get(pk)) {
         fn(pk, pksz, dst);
         return true;
      }
      return false;
   }
};

} // namespaces
#endif//__f9twf_ExgMdFmt_hpp__
