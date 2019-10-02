// \file f9twf/Config.h
// \author fonwinz@gmail.com
#ifndef __f9twf_Config_h__
#define __f9twf_Config_h__
#include "fon9/sys/Config.h"
#ifdef __cplusplus
extern "C" {
#endif

#if defined(fon9_WINDOWS)
   /// C API 的呼叫方式, Windows: __stdcall
   #define f9twf_CAPI_CALL   __stdcall
#else//#if WIN #else...
   /// C API 的呼叫方式, Windows: __stdcall, unix: 空白(不須額外定義).
   #define f9twf_CAPI_CALL
#endif//#if WIN #else #endif

/// 設定 f9twf_API: MSVC 的 DLL import/export.
#if defined(f9twf_NODLL)
   #define f9twf_API
#elif defined(f9twf_EXPORT)
   #define f9twf_API    __declspec(dllexport)
#elif defined(f9twf_WINDOWS)
   #define f9twf_API    __declspec(dllimport)
#else//not defined: fon9_WINDOWS
   #define f9twf_API
#endif

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9twf_Config_h__
