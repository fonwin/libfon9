/// \file fon9/Tools.cpp
/// \author fonwinz@gmail.com
#include "fon9/Tools.hpp"

#ifndef fon9_WINDOWS
#include <iconv.h>
#include <sched.h>
#include <pthread.h>
#endif

namespace fon9 {

static const StrView howWaitStrMap[]{
   fon9_MAKE_ENUM_CLASS_StrView_NoSeq(1, HowWait, Block),
   fon9_MAKE_ENUM_CLASS_StrView_NoSeq(2, HowWait, Yield),
   fon9_MAKE_ENUM_CLASS_StrView_NoSeq(3, HowWait, Busy),
};

fon9_API HowWait StrToHowWait(StrView value) {
   int idx = 0;
   for (const StrView& v : howWaitStrMap) {
      ++idx;
      if (v == value)
         return static_cast<HowWait>(idx);
   }
   return HowWait::Unknown;
}

fon9_API StrView HowWaitToStr(HowWait value) {
   size_t idx = static_cast<size_t>(value) - 1;
   if (idx >= numofele(howWaitStrMap))
      return StrView("Unknown");
   return howWaitStrMap[idx];
}

//--------------------------------------------------------------------------//

fon9_API Result3 SetCpuAffinity(int cpuAffinity) {
   if (cpuAffinity < 0)
      return Result3::kNoResult();
#if defined(fon9_WINDOWS)
   if (SetThreadAffinityMask(GetCurrentThread(), (static_cast<DWORD_PTR>(1) << cpuAffinity)) == 0)
      return GetSysErrC();
#else
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(cpuAffinity, &cpuset);
   if (int iErr = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
      return GetSysErrC(iErr);
#endif
   return Result3::kSuccess();
}

//--------------------------------------------------------------------------//

fon9_API size_t Big5ToUtf8NoEOS(StrView strBig5, char* outbuf, size_t outbufsz) {
   if (outbufsz <= 0)
      return 0;
   size_t szStrBig5 = strBig5.size();
   #if defined(fon9_WINDOWS)
      static thread_local std::wstring wbuf{128, '\0'};
      if (wbuf.size() < szStrBig5 * 2)
         wbuf.resize((szStrBig5 + 128) * 2);
      int   len = MultiByteToWideChar(950, 0,
                                      strBig5.begin(), static_cast<int>(szStrBig5),
                                      &*wbuf.begin(), static_cast<int>(wbuf.size()));
      return static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), len,
                                                     outbuf, static_cast<int>(outbufsz),
                                                     NULL, NULL));
   #else
      static iconv_t iConv = iconv_open("utf8", "big5");
      const char*    pSrc = strBig5.begin();
      char*          pDst = outbuf;
      iconv(iConv, const_cast<char**>(&pSrc), &szStrBig5, &pDst, &outbufsz);
      return static_cast<size_t>(pDst - outbuf);
   #endif
}

} // namespace
