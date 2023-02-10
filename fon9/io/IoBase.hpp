/// \file fon9/io/IoBase.hpp
/// \author fonwinz@gmail.com
/// \defgroup io  IO通訊
#ifndef __fon9_io_IoBase_hpp__
#define __fon9_io_IoBase_hpp__
#include "fon9/io/IoState.h"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/Utility.hpp"
#include "fon9/ConfigParser.hpp"

#include <memory>

namespace fon9 { namespace io {

class Device;
template <class DeviceT>
using DeviceSPT = intrusive_ptr<DeviceT>;
/// \ingroup io
/// Device 的指標, 一般而言在收到 fon9::io::State::Disposing 事件時, 必須要 reset();
using DeviceSP = DeviceSPT<Device>;
class DeviceServer;
class DeviceAcceptedClient;
struct DeviceOpLocker;

class Session;
template <class SessionT>
using SessionSPT = intrusive_ptr<SessionT>;
using SessionSP = SessionSPT<Session>;

class Manager;
using ManagerSP = intrusive_ptr<Manager>;

//--------------------------------------------------------------------------//

/// \ingroup io
/// Device 的類型.
enum class Style {
   /// 不認識的類型.
   Unknown,
   /// 虛擬設備, 例如 DeviceFileIO
   Simulation,
   /// 硬體設備, 例如 USB port, RS232...
   Hardware,
   /// 等候連線端, 例如: TcpServer.
   /// 本身無法收送資料, 僅等候 Client 連入之後建立新的 Device.
   Server,
   /// 由 Server 建立的 Device.
   AcceptedClient,
   /// 發起連線端, 例如: TcpClient.
   Client,
   /// 等候連入端, 例如: 等候連入的 Modem 、等候連入的電話設備...
   LinkIn,
};

/// \ingroup io
/// \copydoc enum f9io_State_t;
enum class State {
   Initializing = f9io_State_Initializing,
   Initialized = f9io_State_Initialized,
   ConfigError = f9io_State_ConfigError,
   Opening = f9io_State_Opening,

   WaitingLinkIn = f9io_State_WaitingLinkIn,
   Linking = f9io_State_Linking,
   LinkError = f9io_State_LinkError,
   LinkReady = f9io_State_LinkReady,
   LinkBroken = f9io_State_LinkBroken,

   Listening = f9io_State_Listening,
   ListenBroken = f9io_State_ListenBroken,

   Lingering = f9io_State_Lingering,
   Closing = f9io_State_Closing,
   Closed = f9io_State_Closed,

   Disposing = f9io_State_Disposing,
   Destructing = f9io_State_Destructing,
};
/// \ingroup io
/// 取得 st 的顯示字串.
inline StrView GetStateStr(State st) {
   return f9io_GetStateStr(static_cast<f9io_State>(st));
}

inline bool IsAllowContinueSend(State st) {
   return st == State::LinkReady || st == State::Lingering;
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// OnDevice_StateUpdated() 事件的參數.
/// State 沒變, 但更新訊息.
/// 例如: 還在 State::Linking, 但是嘗試不同的 RemoteAddress.
struct StateUpdatedArgs {
   fon9_NON_COPY_NON_MOVE(StateUpdatedArgs);
   StateUpdatedArgs(State st, StrView info, const std::string& deviceId)
      : StateUpdatedArgs(info, deviceId) {
      this->State_ = st;
   }
   StateUpdatedArgs(StrView info, const std::string& deviceId)
      : DeviceId_(deviceId)
      , Info_{info} {
   }
   std::string DeviceId_;
   /// if (State_== State::Opening) 則 Info_ = cfgstr;
   StrView     Info_;
   State       State_;
};

/// \ingroup io
/// OnDevice_StateChanged() 事件的參數.
struct StateChangedArgs {
   fon9_NON_COPY_NON_MOVE(StateChangedArgs);
   StateChangedArgs(const StrView& info, const std::string& deviceId)
      : After_{info, deviceId} {
   }
   StateUpdatedArgs After_;
   State            BeforeState_;
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// \ingroup io
/// 接收緩衝區大小.
/// 若要自訂大小, 可自行使用 static_cast<RecvBufferSize>(2048);
enum class RecvBufferSize : int32_t {
   /// 不收資料: 不產生 OnDevice_Recv() 事件.
   NoRecvEvent = -1,
   /// 關閉接收端, 在 Socket Device 可能觸發 shutdown(so, SHUT_RD);
   CloseRecv = -2,

   /// FnOnDevice_RecvDirect_() 處理過程發現斷線, 或 rxbuf 已經無效.
   NoLink = -3,
   /// FnOnDevice_RecvDirect_() 處理過程發現必須使用 Device Async 操作接收事件.
   /// 應到 op thread 觸發 OnDevice_Recv() 事件.
   AsyncRecvEvent = -4,

   /// 交給 Device 自行決定接收緩衝區大小.
   Default = 0,
   /// 最少 128 bytes.
   B128 = 128,
   /// 最少 1024 bytes.
   B1024 = 1024,

   B1K = B1024,
   B4K = B1K * 4,
   B8K = B1K * 8,
   B64K = B1K * 64,
};
class RecvBuffer;

//--------------------------------------------------------------------------//

enum class DeviceFlag {
   SendASAP = 0x01,
   /// 若 `RecvBufferSize OnDevice_LinkReady();` 或 `RecvBufferSize OnDevice_Recv();`
   /// 傳回 RecvBufferSize::NoRecvEvent, 則會關閉 OnDevice_Recv() 事件.
   NoRecvEvent = 0x02,
};
fon9_ENABLE_ENUM_BITWISE_OP(DeviceFlag);

fon9_WARN_DISABLE_PADDING;
struct DeviceOptions {
   DeviceFlag  Flags_{DeviceFlag::SendASAP};

   // 單位:ms, 0 表示 LinkError 時不用 reopen; 預設為 15 秒.
   uint32_t    LinkErrorRetryInterval_{15000};
   // 單位:ms, 0 表示 LinkBroken 時不用 reopen; 預設為 3 秒.
   uint32_t    LinkBrokenReopenInterval_{3000};
   // 單位:ms, 0 表示進入 Closed 狀態後不用 reopen; 預設為 0.
   uint32_t    ClosedReopenInterval_{0};

   /// 設定屬性參數:
   /// - SendASAP=N        預設值為 'Y'，只要不是 'N' 就會設定成 Yes(若未設定，初始值為 Yes)。
   /// - RetryInterval=n   LinkError 之後重新嘗試的延遲時間, 預設值為 15 秒, 0=不要 retry.
   /// - ReopenInterval=n  LinkBroken 或 ListenBroken 之後, 重新嘗試的延遲時間, 預設值為 3 秒, 0=不要 reopen.
   /// - ClosedReopen=n    Closed 之後, 重新開啟的延遲時間, 預設值為 0=不要 reopen.
   ///   使用 TimeInterval 格式設定, 延遲最小單位為 ms, e.g.
   ///   "RetryInterval=3"    表示連線失敗後, 延遲  3 秒後重新連線.
   ///   "ReopenInterval=0.5" 表示斷線後, 延遲  0.5 秒後重新連線.
   ConfigParser::Result OnTagValue(StrView tag, StrView& value);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_IoBase_hpp__
