// \file f9twf/ExgLineTmpLog.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgLineTmpLog.hpp"

namespace f9twf {

void ExgLineTmpLog::CheckL30_EndOutBoundNum(TmpMsgSeqNum_t endn) {
   if (this->Curr_.LastRxMsgSeqNum_ > endn) {
      fon9::RevBufferList rbuf{128};
      fon9::RevPrint(rbuf, "|L30|EndOutBoundNum=", endn, "|my=", this->Curr_.LastRxMsgSeqNum_);
      rbuf.AllocPacket<TmpLogPacketHeader>()
         ->Initialize(TmpLogPacketType::Info, fon9::CalcDataSize(rbuf.cfront()), fon9::UtcNow());
      this->Append(rbuf.MoveOut());
      this->Curr_.LastRxMsgSeqNum_ = endn;
   }
}
std::string ExgLineTmpLog::Open(const ExgLineTmpArgs& lineArgs,
                                std::string           logFileName,
                                fon9::TimeStamp       tday,
                                fon9::FileMode        fmode) {
   if (fmode == fon9::FileMode{})
      fmode = fon9::FileMode::CreatePath | fon9::FileMode::DenyWrite
            | fon9::FileMode::UnsafeAppendWrite
            | fon9::FileMode::Read;
   fon9::ZeroStruct(this->Saved_);
   fon9::ZeroStruct(this->Curr_);
   this->Appender_ = fon9::LogFileAppender::Make();
   auto res = this->Appender_->OpenImmediately(logFileName, fmode);
   fon9::StrView errfn;
   if (res.IsError()) {
      errfn = "Open";
   __OPEN_ERR:;
      this->Appender_.reset();
      return fon9::RevPrintTo<std::string>("f9twf.ExgLineTmpLog.Open"
                                           "|errfn=", errfn,
                                           "|fname=", logFileName,
                                           '|', res);
   }
   res = this->Appender_->GetFileSize();
   if (res.IsError()) {
      errfn = "GetFileSize";
      goto __OPEN_ERR;
   }
   #define kHeaderSize  128
   static const size_t  kRevBufSize = 512;
   char                 hdrbuf[kRevBufSize + kHeaderSize];

   const auto        fsize = res.GetResult();
   static const char kCSTR_Header[] = "f9twf/TaiFex/TMP/V0" "\n";
   const uint32_t    tdayYYYYMMDD = fon9::GetYYYYMMDD(tday);
   if (fsize == 0) {
      memset(hdrbuf + kRevBufSize, 0, kHeaderSize);
      fon9::RevBufferFixedMem rbuf{hdrbuf, kRevBufSize};
      fon9::RevPrint(rbuf, kCSTR_Header,
                     "TDay=", tdayYYYYMMDD,
                     "|FcmId=", TmpGetValueU(lineArgs.SessionFcmId_),
                     "|SessionId=", TmpGetValueU(lineArgs.SessionId_),
                     "|HeaderSize=" fon9_CTXTOCSTR(kHeaderSize) "\n\n");
      assert(rbuf.GetUsedSize() + sizeof(FHeader) <= kHeaderSize);
      this->Appender_->Append(rbuf.GetCurrent(), kHeaderSize);
      return std::string{};
   }
   if (fsize < kHeaderSize) {
      res = std::errc::bad_message;
      errfn = "FileSize";
      goto __OPEN_ERR;
   }
   res = this->Appender_->Read(0, hdrbuf, kHeaderSize);
   if (res.IsError()) {
      errfn = "Read";
      goto __OPEN_ERR;
   }
   if (memcmp(hdrbuf, kCSTR_Header, sizeof(kCSTR_Header) - 1) != 0) {
      res = std::errc::bad_message;
      errfn = "FileHeader";
      goto __OPEN_ERR;
   }
   const char* pinfo = hdrbuf + sizeof(kCSTR_Header) - 1;
   const char* pend = reinterpret_cast<const char*>(memchr(pinfo, '\n', kHeaderSize - (sizeof(kCSTR_Header) - 1)));
   if (pend == nullptr) {
      res = std::errc::bad_message;
      errfn = "FileHeader.2";
      goto __OPEN_ERR;
   }
   // 檢查 Header(TDay,FcmId,SessionId...) 是否正確
   uint32_t       cTDay = 0;
   TmpFcmId_t     cFcmId = 0;
   TmpSessionId_t cSessionId = 0;
   unsigned       cHeaderSize = 0;
   fon9::StrView  info{pinfo, pend};
   fon9::StrView  tag, value;
   while (fon9::StrFetchTagValue(info, tag, value, '|', '=')) {
      #define if_TagValue(Name)  if (tag == #Name) c##Name = fon9::StrTo(value, c##Name)
      if_TagValue(TDay);
      else if_TagValue(FcmId);
      else if_TagValue(SessionId);
      else if_TagValue(HeaderSize);
   }
   if (cTDay != tdayYYYYMMDD
       || cFcmId != TmpGetValueU(lineArgs.SessionFcmId_)
       || cSessionId != TmpGetValueU(lineArgs.SessionId_)
       || cHeaderSize != kHeaderSize) {
      res = std::errc::bad_message;
      errfn = "FileHeader.Info";
      goto __OPEN_ERR;
   }
   // 取得最後儲存的訊息: LastRxMsgSeqNum_ / LastTxMsgSeqNum_ / ...
   memcpy(&this->Saved_, hdrbuf + kHeaderSize - sizeof(this->Saved_), sizeof(this->Saved_));
   this->Curr_.LeCopyFrom(this->Saved_);
   if (this->Curr_.FileSize_ < cHeaderSize)
      this->Curr_.FileSize_ = cHeaderSize;
   if (this->Curr_.FileSize_ == fsize) // 上次結束時, 資料有正確更新, 所以 this->Saved_ 沒問題.
      return std::string{};
   if (this->Curr_.FileSize_ > fsize) {
      res = std::errc::bad_message;
      errfn = "FileHeader.FileSize";
      goto __OPEN_ERR;
   }
   // 從 this->Curr_.FileSize_ 開始, 往尾端讀取, 檢查 LastRxMsgSeqNum, LastTxMsgSeqNum;
   // - 不解開 TmpL41 的回補, 因為處理完回補後, 會呼叫 UpdateLogHeader() 寫入最後狀態.
   // - 如果有收到 TmpL41, 但沒更新最後狀態(沒呼叫 UpdateLogHeader),
   //   => 那就表示可能是 crash, 因此情況極少發生, 若有此情況, 就用回補方式處理吧!
   TmpLogPacketHeader   loghdr;
   TmpHeader            tmphdr;
   while (this->Curr_.FileSize_ < fsize) {
      res = this->Appender_->Read(this->Curr_.FileSize_, &loghdr, sizeof(loghdr));
      if (res.IsError() || res.GetResult() != sizeof(TmpLogPacketHeader)) {
         errfn = "Read.LogPkHdr";
         goto __OPEN_ERR;
      }
      if (memcmp(&loghdr.FF4_, kCSTR_TmpLogPacketHeader, sizeof(loghdr.FF4_)) != 0) {
         res = std::errc::bad_message;
         errfn = "LogPkHdr";
         goto __OPEN_ERR;
      }
      /// Size_ = 實際占用的資料量 = pksz + sizeof(LogHeader);
      auto logsz = TmpGetValueU(loghdr.Size4_);
      switch (loghdr.LogType_) {
      case TmpLogPacketType::Send:
      case TmpLogPacketType::Recv:
         if (logsz <= sizeof(TmpLogPacketHeader) + sizeof(TmpHeader)) {
            res = std::errc::bad_message;
            errfn = "LogPkHdrSize";
            goto __OPEN_ERR;
         }
         res = this->Appender_->Read(this->Curr_.FileSize_ + sizeof(loghdr), &tmphdr, sizeof(tmphdr));
         if (res.IsError() || res.GetResult() != sizeof(TmpHeader)) {
            errfn = "Read.TmpPkHdr";
            goto __OPEN_ERR;
         }
         if (auto seqn = TmpGetValueU(tmphdr.MsgSeqNum_)) {
            if (loghdr.LogType_ == TmpLogPacketType::Send)
               this->Curr_.LastTxMsgSeqNum_ = seqn;
            else
               this->Curr_.LastRxMsgSeqNum_ = seqn;
         }
         break;
      case TmpLogPacketType::Info:
         break;
      }
      this->Curr_.FileSize_ += logsz;
   }
   return std::string{};
}
void ExgLineTmpLog::UpdateLogHeader() {
   auto res = this->Appender_->GetFileSize();
   if (res.IsError())
      return;
   FHeader curr;
   this->Curr_.FileSize_ = res.GetResult();
   curr.LeCopyFrom(this->Curr_);
   if (memcmp(&curr, &this->Saved_, sizeof(curr)) == 0)
      return;
   this->Appender_->Write(kHeaderSize - sizeof(curr), &curr, sizeof(curr));
   this->Saved_ = curr;
}

} // namespaces
