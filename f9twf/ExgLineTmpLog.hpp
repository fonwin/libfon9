// \file f9twf/ExgLineTmpLog.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgLineTmpLog_hpp__
#define __f9twf_ExgLineTmpLog_hpp__
#include "f9twf/ExgLineTmpArgs.hpp"
#include "fon9/LogFile.hpp"

namespace f9twf {

enum class TmpLogPacketType : char {
   Send = 'S',
   Recv = 'R',
   Info = 'I',
};

struct TmpLogPacketHeader {
   /// 固定填 0xff;
   fon9::CharAryF<4>                FF4_;
   TmpLogPacketType                 LogType_;
   /// Size_ = 實際占用的資料量 = pksz + sizeof(LogHeader);
   ByteAry<4>                       Size4_;
   ByteAry<sizeof(fon9::TimeStamp)> TimeStamp_;

   /// pksz 不包含 sizeof(TmpLogPacketHeader);
   void Initialize(TmpLogPacketType type, size_t pksz, fon9::TimeStamp now) {
      pksz += sizeof(TmpLogPacketHeader);
      assert(static_cast<uint32_t>(pksz) == pksz);
      #define kCSTR_TmpLogPacketHeader    "\n\xff\xff\xff"
      memcpy(this->FF4_.Chars_, kCSTR_TmpLogPacketHeader, sizeof(this->FF4_));
      static_assert(sizeof(this->FF4_) == sizeof(kCSTR_TmpLogPacketHeader)-1, "TmpLogPacketType.FF4_ error.");
      this->LogType_ = type;
      TmpPutValue(this->Size4_, static_cast<uint32_t>(pksz));
      TmpPutValue(this->TimeStamp_, now.GetOrigValue());
   }
};
static_assert(sizeof(TmpLogPacketHeader) == 17, "struct TmpLogPacketHeader must pack?");

/// Log format:
/// - file header:
///   - 檔案說明(文字格式):
///     - 固定字串: "f9twf/TaiFex/TMP/V0" "\n"
///     - "TDay=YYYYMMDD/FcmId=/SessionId=/HeaderSize=" "\n\n";
///   - 保留 n bytes, 額外記錄:
///     - FileSize、LastRxMsgSeqNum、LastTxMsgSeqNum
///     - LastRecvPacket、LastSendPacket = 有編序號的最後封包位置;
///     - 寫入時機: 正常結束時; 每 n 秒? 收到R05? 送出R04? 累積了太多Packet?
///     - 重啟時, 透過 FileSize 來決定 LastRecv/LastSend 要從那兒繼續檢查.
///       - 如果 FileSize 正確, 則直接讀取 LastRecv/LastSend 封包, 即可得知最後序號.
///       - 如果 FileSize < Log_->FileSize(), 則讀取 LastRecv/LastSend 封包之後, 從 FileSize 往尾端循序讀取.
///       - 如果 FileSize > Log_->FileSize(): bad_message.
/// - Packet: '\n' + 0xffffff + uint32_t Size; + TimeStamp; + Send/Recv/Info; + TmpPacket
class f9twf_API ExgLineTmpLog {
   struct FHeader {
      using PosType = fon9::File::PosType;
      PosType        FileSize_;
      TmpMsgSeqNum_t LastRxMsgSeqNum_;
      TmpMsgSeqNum_t LastTxMsgSeqNum_;

      void LeCopyFrom(const FHeader& src) {
         fon9::PutLittleEndian(&this->FileSize_,        src.FileSize_);
         fon9::PutLittleEndian(&this->LastRxMsgSeqNum_, src.LastRxMsgSeqNum_);
         fon9::PutLittleEndian(&this->LastTxMsgSeqNum_, src.LastTxMsgSeqNum_);
      }
   };
   static_assert(sizeof(FHeader) == 16, "struct FHeader must pack?");

   fon9::AsyncFileAppenderSP  Appender_;
   FHeader                    Saved_;
   FHeader                    Curr_;
public:
   bool IsReady() const {
      return this->Appender_.get() != nullptr;
   }
   TmpMsgSeqNum_t FetchTxSeqNum() {
      return ++this->Curr_.LastTxMsgSeqNum_;
   }
   TmpMsgSeqNum_t LastRxMsgSeqNum() const {
      return this->Curr_.LastRxMsgSeqNum_;
   }
   /// 若序號不連續, 則返回 false;
   /// 若序號連續, 設定新序號, 則返回 true;
   bool CheckSetRxSeqNum(TmpMsgSeqNum_t seqn) {
      assert(seqn != 0);
      if (fon9_UNLIKELY(seqn != this->Curr_.LastRxMsgSeqNum_ + 1))
         return false;
      this->Curr_.LastRxMsgSeqNum_ = seqn;
      return true;
   }

   /// 返回失敗訊息.
   std::string Open(const ExgLineTmpArgs& lineArgs,
                    std::string           logFileName,
                    fon9::TimeStamp       tday,
                    fon9::FileMode        fmode = fon9::FileMode{});

   void UpdateLogHeader();

   void Append(fon9::BufferList&& outbuf) {
      this->Appender_->Append(std::move(outbuf));
   }
};

} // namespaces
#endif//__f9twf_ExgLineTmpLog_hpp__
