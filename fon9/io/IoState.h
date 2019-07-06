// \file fon9/io/IoState.h
// \author fonwinz@gmail.com
#ifndef __fon9_io_IoState_h__
#define __fon9_io_IoState_h__
#include "fon9/sys/Config.h"
#include "fon9/CStrView.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

/// \ingroup io
/// Device 的狀態變化: 這裡的狀態變化順序與 enum 的數值有關, 不可隨意調整順序.
enum f9io_State_t {
   /// 初始化尚未完成: Device 建構之後的初始狀態.
   f9io_State_Initializing,
   /// 建構 Device 的人必須主動呼叫 Device::Initialize() 才會進入此狀態.
   f9io_State_Initialized,

   /// Device::OpImpl_Open() 因設定錯誤的開啟失敗。
   /// 無法使用相同 cfgstr 再次開啟.
   f9io_State_ConfigError,
   /// 開啟中: 開啟尚未完成.
   /// 若開啟完成, 則進入底下3種可能的狀態, 視不同的 Device Style 而定:
   /// - WaitingLinkIn
   /// - Linking
   /// - Listening
   f9io_State_Opening,

   /// 連入等候中: 例如一個 USB port, 正在等候 USB 設備的插入.
   f9io_State_WaitingLinkIn,
   /// 連線建立中: 例如 TcpClient 呼叫了 connect().
   f9io_State_Linking,
   /// 連線建立失敗: 例如 TcpClient 呼叫 connect() 之後沒有連線成功.
   f9io_State_LinkError,
   /// 連線成功: 可以開始收送資料.
   /// - WaitingLinkIn, Linking 之後, 連線成功的狀態.
   f9io_State_LinkReady,
   /// 連線中斷.
   /// - **LinkReady 之後** , 連線中斷了!
   f9io_State_LinkBroken,

   /// Listening(例如: TcpServer)
   f9io_State_Listening,
   /// Listening之後被中斷: reset 網卡/IP?
   f9io_State_ListenBroken,

   /// 逗留中, 等資料送完後關閉.
   f9io_State_Lingering,
   /// 關閉中.
   f9io_State_Closing,
   /// 已關閉.
   f9io_State_Closed,

   /// 即將解構, Device 已不可再使用!
   /// 當收到此狀態, 您應該盡快將 Device 釋放.
   f9io_State_Disposing,
   /// 在 Device::~Device() 最後的解構狀態.
   f9io_State_Destructing,
};
typedef enum f9io_State_t  f9io_State;

/// \ingroup io
/// 取得 st 的顯示字串.
fon9_API fon9_CStrView f9io_GetStateStr(f9io_State st);

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__fon9_io_IoState_h__
