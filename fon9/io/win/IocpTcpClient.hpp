/// \file fon9/io/win/IocpTcpClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpTcpClient_hpp__
#define __fon9_io_win_IocpTcpClient_hpp__
#include "fon9/io/win/IocpSocketClient.hpp"
#include "fon9/io/TcpClientBase.hpp"

namespace fon9 { namespace io {

struct fon9_API IocpTcpClientImpl : public IocpSocketClientImpl {
   fon9_NON_COPY_NON_MOVE(IocpTcpClientImpl);
   using base = IocpSocketClientImpl;

public:
   using OwnerDevice = TcpClientT<IocpServiceSP, IocpTcpClientImpl>;
   using OwnerDeviceSP = intrusive_ptr<OwnerDevice>;
   const OwnerDeviceSP  Owner_;

   IocpTcpClientImpl(OwnerDevice* owner, Socket&& so, SocketResult& soRes)
      : base(*owner, owner->IoService_, std::move(so), soRes)
      , Owner_{owner} {
   }

   void OpImpl_Close();
   bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);

   virtual void OnIocpSocket_Received(DcQueueList& rxbuf) override;
   virtual void OnIocpSocket_Writable(DWORD bytesTransfered) override;
   virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;
};

//--------------------------------------------------------------------------//

/// \ingroup io
/// Windows TcpClient Device 使用 IOCP.
using IocpTcpClient = DeviceIocp_DeviceStartSend<IocpTcpClientImpl::OwnerDevice, IocpSocket>;

} } // namespaces
#endif//__fon9_io_win_IocpTcpClient_hpp__
