// \file fon9/io/TestDevice.cpp
// \author fonwinz@gmail.com
#ifndef __fon9_io_TestDevice_hpp__
#define __fon9_io_TestDevice_hpp__
#include "fon9/io/Device.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

/// 通常是 UnitTest 拿來測試時使用, 例如: IoFixSession_UT.cpp
class TestDevice : public Device {
   fon9_NON_COPY_NON_MOVE(TestDevice);
   using base = Device;
protected:
   DcQueueList RxBuffer_;
   void SetLinkReady() {
      StrView devid{"R=TestDevice"};
      Device::OpThr_SetDeviceId(*this, devid.ToString());
      Device::OpThr_SetLinkReady(*this, devid.ToString());
   }
   void OpImpl_Open(std::string cfgstr) override {
      fon9_LOG_DEBUG("Device.Open|cfg=", cfgstr);
      this->SetLinkReady();
   }
   void OpImpl_Reopen() override {
      fon9_LOG_DEBUG("Device.Reopen");
      this->SetLinkReady();
   }
   void OpImpl_Close(std::string cause) override {
      fon9_LOG_DEBUG("Device.Close|cause=", cause);
      this->OpImpl_SetState(State::Closed, &cause);
   }

public:
   TestDevice(SessionSP ses, ManagerSP mgr = nullptr, Style style = Style::Simulation, const DeviceOptions* optsDefault = nullptr)
      : base(std::move(ses), std::move(mgr), style, optsDefault) {
   }
   bool IsSendBufferEmpty() const override {
      return true;
   }
   SendResult SendASAP(const void* src, size_t size) override {
      fon9_LOG_DEBUG("Send:", StrView{reinterpret_cast<const char*>(src), size});
      return SendResult{};
   }
   SendResult SendASAP(BufferList&& src) override {
      fon9_LOG_DEBUG("Send:", src);
      size_t srcsz = CalcDataSize(src.cfront());
      DcQueueList{std::move(src)}.PopConsumed(srcsz);
      return SendResult{};
   }
   SendResult SendBuffered(const void* src, size_t size) override {
      return this->SendASAP(src, size);
   }
   SendResult SendBuffered(fon9::BufferList&& src) override {
      return this->SendASAP(std::move(src));
   }
   void OnReceive(const fon9::StrView& str) {
      this->OpQueue_.InplaceOrWait(fon9::AQueueTaskKind::Recv,
                                   DeviceAsyncOp([](Device& dev, std::string rxstr) {
         static_cast<TestDevice*>(&dev)->RxBuffer_.Append(rxstr.c_str(), rxstr.size());
         if (dev.Session_->OnDevice_Recv(dev, static_cast<TestDevice*>(&dev)->RxBuffer_) == RecvBufferSize::NoRecvEvent)
            fon9_LOG_DEBUG("OnReceive.NoRecvEvent");
      }, str.ToString()));
   }
};
using TestDeviceSP = fon9::intrusive_ptr<TestDevice>;

} } // namespaces
#endif//__fon9_io_TestDevice_hpp__
