/// \file fon9/io/win/IocpSocket.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpSocket.hpp"

namespace fon9 { namespace io {

#define IocpASAP_Count  1024
#define IOV_MAX         128

IocpSocket::SendASAPBufferImpl::SendASAPBufferImpl()
   : BufferSize_{IocpASAP_Count}
   , Container_{new OverlappedSendASAP[BufferSize_]} {
   this->FreeList_.resize(this->BufferSize_);
   for (size_t idx = 0; idx < this->BufferSize_; ++idx)
      this->FreeList_[idx] = &this->Container_[idx];
}
IocpSocket::SendASAPBufferImpl::~SendASAPBufferImpl() {
   delete[] this->Container_;
}
void IocpSocket::SendASAPBufferImpl::ForceClearBuffer(const ErrC& errc) {
   for (size_t idx = 0; idx < this->BufferSize_; ++idx) {
      this->Container_[idx].Buffer_.ConsumeErr(errc);
   }
}
void IocpSocket::SendASAPBufferImpl::Free(OverlappedSendASAP* ovbuf) {
   assert(fon9::unsigned_cast(ovbuf - this->Container_) < this->BufferSize_);
   assert(ovbuf->Buffer_.empty());
   if (fon9::unsigned_cast(ovbuf - this->Container_) < this->BufferSize_)
      this->FreeList_.push_back(ovbuf);
}
IocpSocket::OverlappedSendASAP* IocpSocket::SendASAPBufferImpl::Alloc() {
   if (this->FreeList_.empty())
      return nullptr;
   OverlappedSendASAP* retval = this->FreeList_.back();
   this->FreeList_.pop_back();
   return retval;
}
IocpSocket::OverlappedSendASAP* IocpSocket::SendASAPBufferImpl::FromOverlapped(OVERLAPPED* lpOverlapped) {
   const auto idx = fon9::unsigned_cast(static_cast<OverlappedSendASAP*>(lpOverlapped) - this->Container_);
   assert(idx < this->BufferSize_);
   return idx < this->BufferSize_ ? &this->Container_[idx] : nullptr;
}
bool IocpSocket::SendASAPBufferImpl::OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) {
   if (OverlappedSendASAP* find = this->FromOverlapped(lpOverlapped)) {
      find->Buffer_.PopConsumed(bytesTransfered);
      // assert(find->Buffer_.empty()); // Iocp Done, 則必定已全部完成?
      if (find->Buffer_.empty()) {
         this->FreeList_.push_back(find);
         return true;
      }
      //printf("[-----Tx:%u|Remain=%u|nodes=%u]\n", static_cast<unsigned>(bytesTransfered), static_cast<unsigned>(find->Buffer_.CalcSize()), static_cast<unsigned>(find->Buffer_.GetNodeCount()));
   }
   return false;
}

//--------------------------------------------------------------------------//

IocpSocket::IocpSocket(Device& owner, IocpServiceSP iosv, Socket&& so, SocketResult& soRes)
   : IocpHandler{std::move(iosv)}
   , Socket_{std::move(so)}
   , OwnerDevice_(owner) {
   auto res = this->IocpAttach(this->Socket_.GetSocketHandle());
   if (res.IsError())
      soRes = SocketResult{"IocpAttach", res.GetError()};
}
IocpSocket::~IocpSocket() {
   this->SendBufferedBuffer_.ForceClearBuffer(this->Eno_ ? GetSysErrC(this->Eno_) : ErrC{std::errc::operation_canceled});
   this->SendASAPBuffer_.Lock()->ForceClearBuffer(this->Eno_ ? GetSysErrC(this->Eno_) : ErrC{std::errc::operation_canceled});
}
std::string IocpSocket::GetErrorMessage(OVERLAPPED* lpOverlapped, DWORD eno) const {
   std::string errmsg{this->GetOverlappedKind(lpOverlapped).ToString()};
   errmsg.push_back(':');
   if (eno == WSAEDISCON)
      errmsg.append("disconnected.");
   else
      RevPrintAppendTo(errmsg, GetSysErrC(eno));
   return errmsg;
}

void IocpSocket::OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) {
   if (this->Eno_ == 0)
      this->Eno_ = eno;
   this->OnIocpSocket_Error(lpOverlapped, eno);
   this->IocpSocketReleaseRef();
}
bool IocpSocket::DropRecv() {
   DWORD    bytesTransfered = 0;
   char     buf[1024 * 256];
   WSABUF   bufv{sizeof(buf), buf};
   DWORD    rxBytes, flags = 0;
   while (WSARecv(this->Socket_.GetSocketHandle(), &bufv, 1, &rxBytes, &flags, nullptr, nullptr) != SOCKET_ERROR && rxBytes != 0) {
      bytesTransfered += rxBytes;
   }
   int eno = WSAGetLastError();
   if (eno == WSAEWOULDBLOCK || (eno == 0 && bytesTransfered > 0)) {
      this->ContinueRecv(RecvBufferSize::NoRecvEvent);
      return true;
   }
   if (eno == 0 && bytesTransfered == 0) // 正常斷線, 沒有錯誤?!
      eno = WSAEDISCON;
   this->OnIocp_Error(&this->RecvOverlapped_, static_cast<DWORD>(eno));
   return false;
}
void IocpSocket::OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) {
   if (fon9_LIKELY(lpOverlapped == &this->RecvOverlapped_)) {
      if (fon9_LIKELY(bytesTransfered > 0)) {
         DcQueueList& rxbuf = this->RecvBuffer_.SetDataReceived(bytesTransfered);
         this->OnIocpSocket_Received(rxbuf);
      }
      else if (!this->DropRecv()) // DropRecv() 返回失敗, 表示已經呼叫過 OnIocp_Error(); 所以不用再 ReleaseRef();
         return;                  // 因此直接 return;
   }
   else if (lpOverlapped == &this->SendBufferedOverlapped_) {
      this->OnIocpSocket_Writable(bytesTransfered);
   }
   else {
      auto asap = this->SendASAPBuffer_.Lock();
      if (asap->OnIocp_Done(lpOverlapped, bytesTransfered)) {
         if (!asap->Queue_.empty()) {
            //printf("[-----SendQueue:bytes=%u|nodes=%u]\n", static_cast<unsigned>(CalcDataSize(asap->Queue_.cfront())), static_cast<unsigned>(asap->Queue_.size()));
            this->IocpSendASAP(asap, std::move(asap->Queue_));
         }
      }
   }
   this->IocpSocketReleaseRef();
}

//--------------------------------------------------------------------------//

void IocpSocket::StartRecv(RecvBufferSize expectSize) {
   WSABUF bufv[2];
   size_t bufCount;
   if (fon9_UNLIKELY(expectSize < RecvBufferSize::Default)) {
      // 不理會資料事件: Session預期不會有資料.
      // RecvBufferSize::NoRecvEvent, CloseRecv; //還是需要使用[接收事件]偵測斷線.
      bufv[0].len = 0;
      bufv[0].buf = nullptr;
      bufCount = 1;
   }
   else {
      if (expectSize == RecvBufferSize::Default) // 接收緩衝區預設大小.
         expectSize = static_cast<RecvBufferSize>(1024 * 4);
      const size_t allocSize = static_cast<size_t>(expectSize);
      bufCount = this->RecvBuffer_.GetRecvBlockVector(bufv, allocSize >= 64 ? allocSize : (allocSize * 2));
   }
   // 啟動 WSARecv();
   DWORD rxBytes, flags = 0;
   ZeroStruct(this->RecvOverlapped_);
   this->IocpSocketAddRef();
   if (WSARecv(this->Socket_.GetSocketHandle(), bufv, static_cast<DWORD>(bufCount), &rxBytes, &flags, &this->RecvOverlapped_, nullptr) == SOCKET_ERROR) {
      switch (int eno = WSAGetLastError()) {
      case WSA_IO_PENDING: // ERROR_IO_PENDING: 正常接收等候中.
         break;
      case WSAEINVAL: // dgram + NoRecvEvent: 本來就預計不會有 Recv 事件.
         if (expectSize == RecvBufferSize::NoRecvEvent) {
            this->IocpSocketReleaseRef();
            return;
         }
      default: // 接收失敗, 不會產生 OnIocp_* 事件
         this->RecvBuffer_.Clear();
         this->OnIocp_Error(&this->RecvOverlapped_, static_cast<DWORD>(eno));
      }
   }
   else {
      // 已收到(或已從RCVBUF複製進來), 但仍會觸發 OnIocp_Done() 事件,
      // 所以一律在 OnIocp_Done() 事件裡面處理即可.
   }
}

//--------------------------------------------------------------------------//

void IocpSocket::SendASAPBufferImpl::CancelIocpSend(HANDLE so) {
   auto errCanceled = ErrC{std::errc::operation_canceled};
   for (size_t idx = 0; idx < this->BufferSize_; ++idx) {
      CancelIoEx(so, &this->Container_[idx]);
      this->Container_[idx].Buffer_.ConsumeErr(errCanceled);
   }
}
void IocpSocket::CancelIocpSend() {
   CancelIoEx(reinterpret_cast<HANDLE>(this->Socket_.GetSocketHandle()), &this->SendBufferedOverlapped_);
   this->SendASAPBuffer_.Lock()->CancelIocpSend(reinterpret_cast<HANDLE>(this->Socket_.GetSocketHandle()));
}
Device::SendResult IocpSocket::IocpSendASAP(const void* src, size_t size) {
   if (size) {
      fon9::FwdBufferList txbuf(static_cast<BufferNodeSize>(size));
      memcpy(txbuf.AllocBuffer(size), src, size);
      return this->IocpSendASAP(txbuf.MoveOut());
   }
   return Device::SendResult{0};
}
Device::SendResult IocpSocket::IocpSendASAP(BufferList&& src) {
   auto asap = this->SendASAPBuffer_.Lock();
   if (fon9_LIKELY(asap->Queue_.empty()))
      return this->IocpSendASAP(asap, std::move(src));
   asap->Queue_.push_back(std::move(src));
   return Device::SendResult{0};
}
Device::SendResult IocpSocket::IocpSendASAP(SendASAPBuffer::Locker& asap, BufferList&& src) {
   auto* ovbuf = asap->Alloc();
   if (ovbuf == nullptr) {
      asap->Queue_.push_back(std::move(src));
      return Device::SendResult{0};
   }
   assert(ovbuf->Buffer_.empty());
   ovbuf->Buffer_.push_back(std::move(src));
   WSABUF wbuf[IOV_MAX];
   DWORD  wcount = static_cast<DWORD>(ovbuf->Buffer_.PeekBlockVector(wbuf, &asap->Queue_));
   if (fon9_UNLIKELY(wcount == 0)) {
      // 此時的 src 可能為 control node;
   __SEND_BUFFERED:;
      src.push_back(ovbuf->Buffer_.MoveOutToList());
      asap->Free(ovbuf);
      asap.unlock();
      return this->OwnerDevice_.SendBuffered(std::move(src));
   }
   asap.unlock();
   this->IocpSocketAddRef();
   // -----
   ZeroStruct(static_cast<OVERLAPPED*>(ovbuf));
   DWORD  txBytes = 0, flags = 0;
   const int sres = this->SendTo_
      ? WSASendTo(this->Socket_.GetSocketHandle(), wbuf, wcount, &txBytes, flags, &this->SendTo_->Addr_, this->SendTo_->GetAddrLen(), ovbuf, nullptr)
      : WSASend  (this->Socket_.GetSocketHandle(), wbuf, wcount, &txBytes, flags, ovbuf, nullptr);
   if (fon9_LIKELY(sres != SOCKET_ERROR)) {
      // 已送出(已填入SNDBUF), 但仍會觸發 OnIocp_Done() 事件,
      // 所以一律在 OnIocp_Done() 事件裡面處理即可.
      return Device::SendResult{0};
   }
   switch (int eno = WSAGetLastError()) {
   case WSA_IO_PENDING: // ERROR_IO_PENDING: 正常傳送等候中.
      return Device::SendResult{0};
   case WSAEMSGSIZE:
      // 改成使用 SendBuffered();
      this->IocpSocketReleaseRef();
      asap.Relock(this->SendASAPBuffer_);
      //printf("[-----WSAEMSGSIZE: Tx:%u, wcount:%u]\n", static_cast<unsigned>(txBytes), static_cast<unsigned>(wcount));
      goto __SEND_BUFFERED;
   default:
      ovbuf->Buffer_.ConsumeErr(GetSysErrC(static_cast<DWORD>(eno)));
      // 傳送失敗, 不會產生 OnIocp_* 事件, 所以這裡手動呼叫 OnIocp_Error().
      this->OnIocp_Error(ovbuf, static_cast<DWORD>(eno));
      asap.Relock(this->SendASAPBuffer_);
      asap->Free(ovbuf);
      return GetSocketErrC(eno);
   }
}
Device::SendResult IocpSocket::SendAfterAddRef(DcQueueList& dcbuf) {
   ZeroStruct(this->SendBufferedOverlapped_);
   WSABUF wbuf[IOV_MAX];
   DWORD  wcount = static_cast<DWORD>(dcbuf.PeekBlockVector(wbuf));
   if (fon9_UNLIKELY(wcount == 0)) {
      // 避免 WSASend() 真的送出 0 byte 的 socket 封包?
      auto resPost = this->Post(&this->SendBufferedOverlapped_, 0);
      if (resPost)
         return GetSocketErrC(static_cast<int>(resPost));
      return Device::SendResult{0};
      // wcount = 1;
      // wbuf[0].len = 0;
      // wbuf[0].buf = nullptr;
   }

__RETRY_SEND:
   DWORD  txBytes = 0, flags = 0;
   const int sres = this->SendTo_
      ? WSASendTo(this->Socket_.GetSocketHandle(), wbuf, wcount, &txBytes, flags, &this->SendTo_->Addr_, this->SendTo_->GetAddrLen(), &this->SendBufferedOverlapped_, nullptr)
      : WSASend  (this->Socket_.GetSocketHandle(), wbuf, wcount, &txBytes, flags,                                                     &this->SendBufferedOverlapped_, nullptr);
   if (fon9_UNLIKELY(sres == SOCKET_ERROR)) {
      switch (int eno = WSAGetLastError()) {
      case WSA_IO_PENDING: // ERROR_IO_PENDING: 正常傳送等候中.
         break;
      case WSAEMSGSIZE:
         if (wcount > 1) {
            wcount = 1;
            goto __RETRY_SEND;
         }
         // 如果 wcount==1, 則表示 wbuf[0] 封包太大, 應調整打包規則?
         if (wbuf[0].len > 512) {
            wbuf[0].len = 512;
            goto __RETRY_SEND;
         }
         // 不用 break; 視為失敗.
      default:
         this->SendBufferedBuffer_.ForceClearBuffer(GetSysErrC(static_cast<DWORD>(eno)));
         // 傳送失敗, 不會產生 OnIocp_* 事件, 所以這裡手動呼叫 OnIocp_Error().
         this->OnIocp_Error(&this->SendBufferedOverlapped_, static_cast<DWORD>(eno));
         return GetSocketErrC(eno);
      }
   }
   else {
      // 已送出(已填入SNDBUF), 但仍會觸發 OnIocp_Done() 事件,
      // 所以一律在 OnIocp_Done() 事件裡面處理即可.
   }
   return Device::SendResult{0};
}

//--------------------------------------------------------------------------//

SendDirectResult IocpSocket::IocpRecvAux::SendDirect(RecvDirectArgs& e, BufferList&& txbuf) {
   IocpSocket& so = ContainerOf(RecvBuffer::StaticCast(e.RecvBuffer_), &IocpSocket::RecvBuffer_);
   //SendASAP_AuxBuf  aux{txbuf};
   //return DeviceSendDirect(e, so.SendBuffer_, aux);
   auto res = so.IocpSendASAP(std::move(txbuf));
   return res.IsError() ? SendDirectResult::SendError : SendDirectResult::Sent;
}

} } // namespaces
