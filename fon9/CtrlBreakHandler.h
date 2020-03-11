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

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__fon9_CtrlBreakHandler_h__
