// \file fon9/PkReceiver.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_PkReceiver_hpp__
#define __fon9_PkReceiver_hpp__
#include "fon9/buffer/DcQueue.hpp"

namespace fon9 {

/// \ingroup Misc.
/// 用來解析簡易的封包格式框架(例: 交易所行情格式: 台灣證交所、台灣期交所):
/// - kPkHeadLeader(EscCode:27) ... CheckSum + TerminalCode(0x0d,0x0a).
///   - CheckSum = XOR(kPkHeadLeader...CheckSum)
///     - kPkHeadLeader(不含)之後, CheckSum(不含)之前, 每個 byte 之 XOR.
/// - 每個資訊來源(io Session)對應一個 PkReceiver.
/// - io Device 收到資料時, 丟到 PkReceiver::FeedBuffer()
/// - 當收到一個封包時透過 PkReceiver::OnPkReceived() 通知衍生者處理.
class fon9_API PkReceiver {
   fon9_NON_COPY_NON_MOVE(PkReceiver);

public:
   const unsigned PkHeadSize_;
   /// 若使用 IsDgram_ 則 FeedBuffer(rxbuf) 每次都只會收到一個完整封包,
   /// 若返回前 rxbuf 有剩餘, 表示資料有誤, 直接拋棄剩餘資料.
   const bool     IsDgram_;

   PkReceiver(unsigned pkHeadSize, bool isDgram = false)
      : PkHeadSize_{pkHeadSize}
      , IsDgram_{isDgram} {
   }
   virtual ~PkReceiver();

   enum {
      kMaxPacketSize = 1024 * 64,
      kPkHeadLeader = 27,
   };

   /// 收到的訊息透過這裡處理.
   /// - 返回時機: rxbuf 用完, 或 rxbuf 剩餘資料不足一個封包.
   /// - 封包框架正確 & CheckSum 正確, 則會呼叫 OnPkReceived() 通知衍生者.
   ///
   /// \retval false 呼叫 OnPkReceived() 時返回 false, 中斷 FeedBuffer();
   /// \retval true  rxbuf 資料不足, 或已解析完畢.
   bool FeedBuffer(DcQueue& rxbuf);

   static char CalcCheckSum(const char* pkL, unsigned pksz) {
      const char* pkend = pkL + pksz - 4; // -4 = 排除 kPkHeadLeader, CheckSum, 0x0d, 0x0a.
      char        cks = *++pkL; // ++pk = 排除 kPkHeadLeader.
      while (pkL != pkend) {
         fon9_GCC_WARN_DISABLE("-Wconversion");
         cks ^= *++pkL;
         fon9_GCC_WARN_POP;
      }
      return cks;
   }

   void ClearStatus() {
      this->ReceivedCount_ = 0;
      this->ChkSumErrCount_ = 0;
      this->DroppedBytes_ = 0;
   }

   /// this->OnPkReceived(); 的呼叫次數.
   uint64_t GetReceivedCount()  const { return this->ReceivedCount_;  }
   uint64_t GetChkSumErrCount() const { return this->ChkSumErrCount_; }
   uint64_t GetDroppedBytes()   const { return this->DroppedBytes_;   }

protected:
   char     Padding___[3];
   uint64_t ReceivedCount_{0};
   uint64_t ChkSumErrCount_{0};
   uint64_t DroppedBytes_{0};

   /// 當收到 Head 時通知, 由衍生者計算封包大小.
   /// 返回完整封包大小(包含: Head、Body、Tail).
   virtual unsigned GetPkSize(const void* pkptr) = 0;
   /// 當收到封包時透過這裡通知衍生者.
   /// \retval false 結束 FeedBuffer();
   /// \retval true  繼續 FeedBuffer();
   virtual bool OnPkReceived(const void* pk, unsigned pksz) = 0;
};

} // namespaces
#endif//__fon9_PkReceiver_hpp__
