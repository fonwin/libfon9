// \file fon9/ThreadTools.cpp
// \author fonwinz@gmail.com
#include "fon9/ThreadTools.hpp"

namespace fon9 {

#ifdef fon9_WINDOWS

const DWORD MS_VC_EXCEPTION = 0x406D1388;

fon9_WARN_DISABLE_PADDING
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
fon9_WARN_POP

void SetCurrentThreadName(const char* threadName) {
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = (DWORD)-1;
   info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
   __try {
      RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
   }
#pragma warning(pop)
}
#endif

} // namespaces
