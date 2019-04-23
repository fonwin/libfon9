/// \file f9extests/Config.h
///
/// libfon9 C API 的一些基本設定.
///
/// \author fonwinz@gmail.com
#ifndef __f9extests_Config_h__
#define __f9extests_Config_h__
#include "fon9/sys/Config.h"

#ifdef __cplusplus
extern "C" {
#endif

   #if defined(f9extests_NODLL)
      #define f9extests_API
   #elif defined(f9extests_EXPORT)
      #define f9extests_API   __declspec(dllexport)
   #elif defined(fon9_WINDOWS)
      #define f9extests_API   __declspec(dllimport)
   #else//not defined: fon9_WINDOWS
      #define f9extests_API
   #endif


#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9extests_Config_h__
