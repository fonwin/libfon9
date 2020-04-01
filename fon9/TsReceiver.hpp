// \file fon9/TsReceiver.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_TsReceiver_hpp__
#define __fon9_TsReceiver_hpp__
#include "fon9/buffer/DcQueue.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 {

/// \ingroup Misc.
/// 用來解析使用 TsAppend 寫入的封包.
class fon9_API TsReceiver {
   fon9_NON_COPY_NON_MOVE(TsReceiver);

public:
   using PkszT = uint16_t;
   enum : size_t {
      kHeadSize = sizeof(TimeStamp) + sizeof(PkszT)
   };

   TsReceiver() = default;
   virtual ~TsReceiver();

   /// 收到的訊息透過這裡處理.
   /// - 返回時機: rxbuf 用完, 或 rxbuf 剩餘資料不足一個封包.
   ///
   /// \retval false 呼叫 OnPkReceived() 時返回 false, 中斷 FeedBuffer();
   /// \retval true  rxbuf 資料不足, 或已解析完畢.
   bool FeedBuffer(DcQueue& rxbuf);

protected:
   /// 當收到封包時透過這裡通知衍生者.
   /// static_cast<const char*>(pk) - kHeadSize 必定為 head + pk 的內容.
   ///
   /// \retval false 結束 FeedBuffer();
   /// \retval true  繼續 FeedBuffer();
   virtual bool OnPkReceived(TimeStamp pktm, const void* pk, PkszT pksz) = 0;
};

} // namespaces
#endif//__fon9_TsReceiver_hpp__
