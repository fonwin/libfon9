﻿/// \file fon9/CTools.h
/// \author fonwinz@gmail.com
#ifndef __fon9_CTools_h__
#define __fon9_CTools_h__
#include "fon9/sys/Config.h"
#include <inttypes.h>
//--------------------------------------------------------------------------//
fon9_BEFORE_INCLUDE_STD;
#ifdef fon9_WINDOWS
#  include <Windows.h>
#  include <thr/xtimec.h>
#else // fon9_WINDOWS..else
#  include <unistd.h>
#  include <sys/time.h>
#endif
fon9_AFTER_INCLUDE_STD;
//--------------------------------------------------------------------------//
#ifdef __cplusplus
extern "C" {
#endif

#ifdef fon9_WINDOWS
static inline void fon9_SleepMS(unsigned ms) {
   Sleep(ms);
}
static inline uint64_t fon9_GetSystemUS(void) {
   #define DELTA_EPOCH_IN_USEC  11644473600000000ui64
   FILETIME  ft;
   GetSystemTimePreciseAsFileTime(&ft);
   return((((uint64_t)(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) + 5) / 10 - DELTA_EPOCH_IN_USEC;
}
#else // fon9_WINDOWS..else
static inline void fon9_SleepMS(useconds_t ms) {
   usleep(ms * 1000);
}
static inline uint64_t fon9_GetSystemUS(void) {
   struct timeval tv;
   gettimeofday(&tv, (struct timezone*)NULL);
   return (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)tv.tv_usec;
}
#endif
//--------------------------------------------------------------------------//
/// \ingroup Misc
/// 傳回 pbeg 字串中, 首個「!isspace()」或 EOS 的位置.
extern fon9_API char* fon9_CAPI_CALL fon9_StrTrimHead(char* pbeg);

/// \ingroup Misc
/// 傳回 [pbeg..pend) 的字串中, 首個尾端「!isspace(*p); 時 p + 1」或 pbeg 的位置.
extern fon9_API char* fon9_CAPI_CALL fon9_StrTrimTail(char* pbeg, char* pend);

/// \ingroup Misc
/// 取出由空白(isspace)分隔的字串.
/// # retval = StrTrimHead(pbeg);
/// # 尋找 retval 之後的第一個空白(isspace) 或 EOS.
/// # 若找到空白 isspace(*p), 則將第一個空白改成 EOS. 且 *pnext = StrTrimHead(p + 1);
/// # 若沒找到空白, 則 *pnext = NULL;
extern fon9_API char* fon9_CAPI_CALL fon9_StrCutSpace(char* pbeg, char** pnext);

/// \ingroup Misc
/// 從 pbeg(EOS字串) 取出一個字串單元, 由 delims(EOS字串) 定義分隔字元.
/// 返回 pbeg; *ppend = 首個 delims 定義的分隔字元位置 或 EOS 位置;
extern fon9_API const char* fon9_CAPI_CALL fon9_StrFetchNoTrim(
   const char* const pbeg,
   const char** ppend,
   const char* const delims);

//--------------------------------------------------------------------------//
/// 同 Log.hpp 裡面的 enum class LogLevel;
/// 這裡寫個介面給 C API 使用.
fon9_ENUM(fon9_LogLevel, uint8_t) {
   /// 追蹤程式運行用的訊息.
   fon9_LogLevel_Trace,
   /// 抓蟲用的訊息.
   fon9_LogLevel_Debug,
   /// 一般資訊.
   fon9_LogLevel_Info,
   /// 重要訊息, 例如: thread start.
   fon9_LogLevel_Important,
   /// 警告訊息.
   fon9_LogLevel_Warn,
   /// 錯誤訊息.
   fon9_LogLevel_Error,
   /// 嚴重錯誤.
   fon9_LogLevel_Fatal,
};
extern fon9_API void fon9_CAPI_CALL fon9_SetLogLevel(fon9_LogLevel lv);
extern fon9_API fon9_LogLevel fon9_CAPI_CALL fon9_GetLogLevel(void);
//--------------------------------------------------------------------------//

#ifdef __cplusplus
}// extern "C"
#endif
#endif//__fon9_CTools_h__
