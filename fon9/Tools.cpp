/// \file fon9/Tools.cpp
/// \author fonwinz@gmail.com
#include "fon9/Tools.hpp"

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

} // namespace
