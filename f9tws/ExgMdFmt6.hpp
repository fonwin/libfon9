// \file f9tws/ExgMdFmt6.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt6_hpp__
#define __f9tws_ExgMdFmt6_hpp__
#include "f9tws/ExgMdFmt.hpp"

namespace f9tws {
fon9_PACK(1);

/// Fmt6(整股), Fmt23(零股) 的相同部分.
struct ExgMdFmtRtBase : public ExgMdHead {
   StkNo             StkNo_;
   ExgMdHHMMSSu6     Time_;

   /// 股票代號 "000000" 且撮合時間 "999999999999",
   /// 表示普通股交易末筆即時行情資料已送出.
   /// 這裡僅檢查 Time_.HH_ == 0x99;
   /// 因為 HH_ == 0x99 為不合理的時間, 在正常交易訊息, 不可能發生.
   bool IsLastDealSent() const {
      return this->Time_.HH_[0] == 0x99;
   }

   /// - Bit 7 成交
   ///   - 0︰無成交價、成交量，不傳送
   ///   - 1︰有成交價、成交量，而且傳送
   /// - Bit 6-4 買進
   ///   - 000︰無買進價、買進量，不傳送
   ///   - 001－101︰揭示買進價、買進量之傳送之檔位數（1..5檔）
   /// - Bit 3-1 賣出
   ///   - 000︰無賣出價、賣出量，不傳送
   ///   - 001 －101︰揭示賣出價、賣出量之傳送之檔位數（1..5檔）
   /// - Bit 0 最佳五檔價量 (Fmt23 保留)
   ///   逐筆交易每筆委託撮合後，可能產生數個成交價量揭示，
   ///   揭示最後一個成交價量時，同時揭露最佳五檔買賣價量，Bit 0 = 0。
   ///   非最後一個成交價量揭示時，則僅揭示成交價量但不揭示最佳五檔，Bit 0 = 1。
   fon9::byte        ItemMask_;
   /// 以各別 Bit 分別表示各項漲跌停註記、暫緩撮合瞬間價格趨勢及延後收盤註記（預設值均為 0x00）
   /// - Bit 7-6 成交漲跌停註記 00：一般成交; 01：跌停成交; 10：漲停成交
   /// - Bit 5-4 最佳一檔買進   00：一般買進; 01：跌停買進; 10：漲停買進
   /// - Bit 3-2 最佳一檔賣出   00：一般賣出; 01：跌停賣出; 10：漲停賣出
   /// - Bit 1-0 瞬間價格趨勢   00：一般揭示; 01：暫緩撮合且瞬間趨跌; 10：暫緩撮合且瞬間趨漲; 11：（保留）
   fon9::byte        LmtMask_;
   /// - Bit 7 試算狀態註記
   ///   - 若 Bit 7 為 1，表示目前即時行情:PQs_[] 為試算階段狀態；
   ///   - 若 Bit 7 為 0，表示目前為一般揭示狀態，此時 Bit 6 與 Bit 5 註記資料無任何意義。
   /// - Bit 6 試算後,延後開盤註記  0：否; 1：是     (Fmt23 保留)
   /// - Bit 5 試算後,延後收盤註記  0：否; 1：是     (Fmt23 保留)
   /// - Bit 4 撮合方式註記        0：集合競價; 1：逐筆撮合
   /// - Bit 3 開盤註記            0：否; 1：是
   /// - Bit 2 收盤註記            0：否; 1：是
   /// - Bit 1-0 保留
   fon9::byte        StatusMask_;
};

struct ExgMdFmt6v4 : public ExgMdFmtRtBase {
   /// 累計成交數量.
   fon9::PackBcd<8>  TotalQty_;
   /// 根據 ItemMask_ 決定 PQs_[] 有多少元素及其內涵.
   ExgMdPriQty       PQs_[1];

   bool HasBS() const {
      return (this->ItemMask_ & 0x7e) || (this->ItemMask_ & 1) == 0;
   }
};
static_assert(sizeof(ExgMdFmt6v4)
              == sizeof(ExgMdHead) + sizeof(StkNo) + sizeof(ExgMdHHMMSSu6) + sizeof(ExgMdTail)
               + sizeof(ExgMdFmt6v4::TotalQty_) + sizeof(ExgMdFmt6v4::PQs_),
              "ExgMdFmt6v4 沒有 pack?");

fon9_PACK_POP;

//--------------------------------------------------------------------------//
template <class DstPQ, class ExgMdPQ, unsigned kBSCount>
inline const ExgMdPQ* AssignBS(DstPQ (&adst)[kBSCount], const ExgMdPQ* pqs, unsigned count) {
   if (count > kBSCount)
      count = kBSCount;
   DstPQ* pdst = adst;
   for (unsigned L = 0; L < count; ++L) {
      pqs->AssignTo(*pdst);
      ++pdst;
      ++pqs;
   }
   if (count < kBSCount)
      memset(static_cast<void*>(pdst), 0, sizeof(*pdst) * (kBSCount - count));
   return pqs;
}

} // namespaces
#endif//__f9tws_ExgMdFmt6_hpp__
