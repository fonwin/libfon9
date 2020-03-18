﻿// \file fon9/rc/RcClientApi.h
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcClientApi_h__
#define __fon9_rc_RcClientApi_h__
#include "fon9/io/IoState.h"
#include "fon9/rc/Rc.h"
#include <stddef.h> // offsetof();
#include <string.h> // memset();

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

/// 啟動 fon9 C API 函式庫.
/// - 請使用時注意: 不考慮 multi thread 同時呼叫 fon9_Initialize()/fon9_Finalize();
/// - 可重覆呼叫 fon9_Initialize(), 但須有對應的 fon9_Finalize();
///   最後一個 fon9_Finalize() 被呼叫時, 結束函式庫.
/// - 啟動 Device factory.
/// - 啟動 LogFile: 
///   - if (logFileFmt == NULL) 表示不改變 log 的輸出位置, 預設 log 輸出在 console.
///   - if (*logFileFmt == '\0') 則 logFileFmt = "./logs/{0:f+'L'}/fon9cli.log";
///     {0:f+'L'} 表示使用 local time 的 YYYYMMDD.
/// - 若返回非 0:
///   - ENOSYS: 已經呼叫 fon9_Finalize() 系統結束.
///   - 其他為 log 檔開啟失敗的原因(errno).
fon9_CAPI_FN(int) fon9_Initialize(const char* logFileFmt);

/// 結束 fon9 函式庫.
/// - 結束前必須先將所有建立的物件刪除:
///   - 例如: f9rc_CreateClientSession() => f9rc_DestroyClientSession();
/// - 結束系統後, 無法透過 fon9_Initialize() 重啟, 若要重啟, 必須結束程式重新執行.
/// - 返回 0 表示已結束, 無法再呼叫 fon9_Initialize() 重新啟動.
/// - 返回 >0 表示還有多少次, 已呼叫 fon9_Initialize(), 但尚未呼叫 fon9_Finalize();
fon9_CAPI_FN(uint32_t) fon9_Finalize(void);

// typedef void (fon9_CAPI_CALL *fon9_FnFinalizeHandler)(void* userData);
// /// 註冊在最後一個 fon9_Finalize() 被呼叫時, 要處理的事項.
// /// - 正在結束時的註冊, 不會被呼叫.
// /// - 此函式 thread safe.
// ///
// /// \retval 0 註冊失敗: 重複註冊(fnFinalizeHandler 及 userData 皆相同).
// /// \retval 1 註冊成功.
// fon9_CAPI_FN(int) fon9_AddFinalizeHandler(fon9_FnFinalizeHandler fnFinalizeHandler, void* userData);

//--------------------------------------------------------------------------//

/// Rc protocol 可以支援多種功能呼叫(例: OmsRc, SeedVisitor...)
/// 每種功能呼叫, 可能需要 API 的使用者提供: 必要資料, 及相關的 Handler.
/// \code
///   typedef struct {
///      f9rc_FunctionNoteParams BaseParam_;
///      ... 其他參數 ...
///   } f9rc_SeedVisitorNoteParams;
/// \endcode
typedef struct {
   /// sizeof(f9rc_SeedVisitor);
   /// sizeof(f9OmsRc_ClientSessionParams);
   uint32_t          ParamSize_;
   f9rc_FunctionCode FunctionCode_;
   char              Padding__[3];
} f9rc_FunctionNoteParams;

/// 設定需要記錄那些 log.
/// - 0x0007 for RcSession.
/// - 0x00f8 for SeedVisitor
/// - 0x0f00 for OmsRc
fon9_ENUM(f9rc_ClientLogFlag, uint32_t) {
   f9rc_ClientLogFlag_None = 0,
   f9rc_ClientLogFlag_All = 0xffff,

   /// 連線(斷線)相關事件.
   f9rc_ClientLogFlag_Link = 0x0001,
};

typedef struct {
   void*                UserData_;
   f9rc_ClientLogFlag   LogFlags_;
   char                 Reserved___[4];
} f9rc_ClientSession;

typedef void (fon9_CAPI_CALL *f9rc_FnOnLinkEv)(f9rc_ClientSession*, f9io_State st, fon9_CStrView info);

/// 建立 RcClientSession 連線時所需要的參數.
typedef struct {
   const char*          UserId_;
   const char*          Password_;
   const char*          DevName_;
   const char*          DevParams_;
   void*                UserData_;
   f9rc_RcFlag          RcFlags_;
   char                 Reserved___[2];
   f9rc_ClientLogFlag   LogFlags_;

   /// 連線有關的事件: 連線失敗, 連線後斷線...
   f9rc_FnOnLinkEv      FnOnLinkEv_;

   /// 各個 FunctionCode 所需要的參數.
   f9rc_FunctionNoteParams*   FunctionNoteParams_[f9rc_FunctionCode_Count];

} f9rc_ClientSessionParams;

#define f9rc_InitClientSessionParams(f9rcClientSessionParams, f9rcFunctionNoteParams, funcCode) {  \
   memset(f9rcFunctionNoteParams, 0, sizeof(*f9rcFunctionNoteParams));                             \
   f9rcFunctionNoteParams->BaseParams_.FunctionCode_ = funcCode;                                   \
   f9rcFunctionNoteParams->BaseParams_.ParamSize_ = sizeof(*f9rcFunctionNoteParams);               \
   f9rcClientSessionParams->FunctionNoteParams_[funcCode] = &f9rcFunctionNoteParams->BaseParams_;  \
}

//--------------------------------------------------------------------------//

/// 建立一個 rc 連線物件, 當您不再使用時, 應呼叫 f9rc_DestroyClientSession(*result) 銷毀.
/// - 除了 args->UserData_ 可以為任意值(包含NULL), 其他指標若為 NULL, 則直接 crash!
/// - args->DevName_: 通訊設備名稱, 例如: "TcpClient";
/// - args->DevParams_: 通訊設備參數, 例如: "192.168.1.1:6601"; "dn=localhost:6601"
/// - 有可能在返回前就觸發事件, 但此時 *result 必定已經填妥已建立的 Session.
/// - retval 1 成功: *result 儲存 Session;
/// - retval 0 失敗: devName 不正確, 或尚未呼叫 fon9_Initialize();
fon9_CAPI_FN(int)
f9rc_CreateClientSession(f9rc_ClientSession** result,
                         const f9rc_ClientSessionParams* args);

/// 銷毀一個 rc 連線物件.
/// 在返回前, 仍然可能在其他 thread 收到事件.
/// isWait = 1 表示要等確定銷毀後才返回.
/// isWait = 0 表示在返回後仍可能收到事件, 如果您是在事件通知時呼叫, 則不能等候銷毀(會造成死結).
fon9_CAPI_FN(void)
f9rc_DestroyClientSession(f9rc_ClientSession* ses, int isWait);

static inline void f9rc_DestroyClientSession_Wait(f9rc_ClientSession* ses) {
   f9rc_DestroyClientSession(ses, 1);
}
static inline void f9rc_DestroyClientSession_NoWait(f9rc_ClientSession* ses) {
   f9rc_DestroyClientSession(ses, 0);
}

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__fon9_rc_RcClientApi_h__
