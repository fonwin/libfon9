// \file fon9/CtrlBreakHandler.h
// fon9 console
// \author fonwinz@gmail.com
#ifndef __fon9_CtrlBreakHandler_h__
#define __fon9_CtrlBreakHandler_h__
#include "fon9/sys/Config.h"

#ifdef __cplusplus
extern "C" {
#endif

   extern fon9_API const char* fon9_AppBreakMsg;
   extern fon9_API void fon9_CAPI_CALL fon9_SetupCtrlBreakHandler(void);

   /// 使用者手動結束程式, 主程式必須使用 while(fon9_AppBreakMsg == NULL) 迴圈;
   /// 此處會關閉 stdin; 中斷正在等後使用者輸入的地方.
   /// 您必須自行確保 appBreakMsg 在程式 main() 結束前都持續有效!
   extern fon9_API void fon9_CAPI_CALL fon9_BreakApp(const char* appBreakMsg);

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__fon9_CtrlBreakHandler_h__
