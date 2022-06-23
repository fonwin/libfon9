// \file f9tws/ExgMdFmt.hpp
//
// TSE/OTC 行情格式解析, 參考文件:
// - 臺灣證券交易所 / 資訊傳輸作業手冊 (IP 行情網路)
// - 中華民國證券櫃檯買賣中心 / 資訊傳輸作業手冊 （上櫃股票 IP 行情網路）
//
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt_hpp__
#define __f9tws_ExgMdFmt_hpp__
#include "f9tws/ExgTypes.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/fmkt/TwExgMdTime.hpp"

fon9_BEFORE_INCLUDE_STD
#include <memory>
fon9_AFTER_INCLUDE_STD

namespace f9tws {
fon9_PACK(1);

using ExgMdHHMMSSu6 = fon9::fmkt::TwExgMdTimeHHMMSSu6;
using ExgMdHHMMSS = fon9::fmkt::TwExgMdTimeHHMMSS;

enum ExgMdMarket : uint8_t {
   ExgMdMarket_TwTSE = 1,
   ExgMdMarket_TwOTC = 2,
   ExgMdMarket_MaxNo = 2,
};

/// 每個封包的基本框架:
/// - Esc: 1 byte = 27;
/// - fon9::PackBcd<4> Length_; 包含: Esc, 及 ExgMdTail.
struct ExgMdHead {
   fon9::byte        Esc_;
   fon9::PackBcd<4>  Length_;
   /// 0x01=TSE, 0x02=OTC
   fon9::PackBcd<2>  Market_;
   fon9::PackBcd<2>  FmtNo_;
   fon9::PackBcd<2>  VerNo_;
   fon9::PackBcd<8>  SeqNo_;

   ExgMdMarket GetMarket() const {
      assert((this->Market_[0] & 0xf0) == 0);
      static_assert(sizeof(this->Market_) == 1, "");
      return static_cast<ExgMdMarket>(this->Market_[0]);
   }
   unsigned GetLength() const { return fon9::PackBcdTo<unsigned>(this->Length_); }
   uint8_t  GetFmtNo() const { return fon9::PackBcdTo<uint8_t>(this->FmtNo_); }
   uint8_t  GetVerNo() const { return fon9::PackBcdTo<uint8_t>(this->VerNo_); }
   uint32_t GetSeqNo() const { return fon9::PackBcdTo<uint32_t>(this->SeqNo_); }
};
static_assert(sizeof(ExgMdHead) == 10, "ExgMdHead 沒有 pack?");
enum : size_t {
   kExgMdMaxPacketSize = fon9::DecDivisor<size_t, sizeof(ExgMdHead::Length_) * 2>::Divisor,
   kExgMdMaxFmtNoSize = fon9::DecDivisor<size_t, sizeof(ExgMdHead::FmtNo_) * 2>::Divisor,
};

struct ExgMdPriQty {
   /// 2020/03 實施 B.12.00 版本, 價格欄位改成 9(5)V9(4).
   fon9::PackBcd<9>  PriV4_;
   fon9::PackBcd<8>  Qty_;

   void AssignTo(fon9::fmkt::PriQty& dst) const {
      dst.Pri_.Assign<4>(fon9::PackBcdTo<uint64_t>(this->PriV4_));
      dst.Qty_ = fon9::PackBcdTo<fon9::fmkt::Qty>(this->Qty_);
   }
};
static_assert(sizeof(ExgMdPriQty) == 9, "ExgMdPriQty 沒有 pack?");

/// - 檢查碼 1 byte; (Esc...檢查碼)之間(不含Esc、檢查碼)的 XOR 值.
/// - 結束碼 2 bytes; [0]=0x0d; [1]=0x0a;
struct ExgMdTail {
   uint8_t  CheckSum_;
   char     CrLn_[2];
};

fon9_PACK_POP;

//--------------------------------------------------------------------------//

enum {
   kExgMdMaxFmtNo = 30,
   kExgMdMaxVer = 9,
};

/// 透過 [Mkt(1=TWSE,2=OTC)][FmtNo=1..21][Ver=1..8] 來尋找 handler.
template <class HandlerT>
class ExgMdMessageDispatcher {
   fon9_NON_COPY_NON_MOVE(ExgMdMessageDispatcher);

   HandlerT HandlerMap_[ExgMdMarket_MaxNo + 1][kExgMdMaxFmtNo + 1][kExgMdMaxVer + 1];
   HandlerT NullHandler_{};

public:
   ExgMdMessageDispatcher() {
      memset(this->HandlerMap_, 0, sizeof(this->HandlerMap_));
   }

   template <ExgMdMarket mkt, uint8_t fmt, uint8_t ver>
   void Reg(HandlerT handler) {
      static_assert(mkt <= ExgMdMarket_MaxNo && fmt <= kExgMdMaxFmtNo && ver <= kExgMdMaxVer, "");
      assert(!this->HandlerMap_[mkt][fmt][ver]); // 只能註冊一次.
      this->HandlerMap_[mkt][fmt][ver] = std::move(handler);
   }

   /// DailyClear 時, 會需要對全部的 handler 清除最後序號.
   /// 所以必須提供 begin(), end();
   const HandlerT* begin() const {
      return &this->HandlerMap_[0][0][0];
   }
   const HandlerT* end() const {
      return this->begin() + (sizeof(this->HandlerMap_) / sizeof(HandlerT));
   }

   template <class HeadT>
   HandlerT& Get(const HeadT& pk) {
      assert(pk.GetMarket() <= ExgMdMarket_MaxNo
             && pk.GetFmtNo() <= kExgMdMaxFmtNo
             && pk.GetVerNo() <= kExgMdMaxVer);
      if (fon9_LIKELY(pk.GetMarket() <= ExgMdMarket_MaxNo
                      && pk.GetFmtNo() <= kExgMdMaxFmtNo
                      && pk.GetVerNo() <= kExgMdMaxVer))
         return this->HandlerMap_[pk.GetMarket()][pk.GetFmtNo()][pk.GetVerNo()];
      return this->NullHandler_;
   }
   template <class HeadT>
   const HandlerT& Get(const HeadT& pk) const {
      return const_cast<ExgMdMessageDispatcher*>(this)->Get(pk);
   }
};

} // namespaces
#endif//__f9tws_ExgMdFmt_hpp__
