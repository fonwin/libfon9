/// \file fon9/ErrC.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ErrC_hpp__
#define __fon9_ErrC_hpp__
#include "fon9/sys/Config.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <system_error>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

using ErrC = std::error_condition;

#ifdef fon9_WINDOWS
fon9_API ErrC GetSysErrC(DWORD eno = ::GetLastError());
#else
fon9_API ErrC GetSysErrC(int eno = errno);
#endif

template <class RevBuffer>
void RevPrint(RevBuffer& rbuf, ErrC errc) {
   RevPrint(rbuf, ':', errc.value(), ':', errc.message());
   if (errc.category() != std::generic_category())
      RevPrint(rbuf, errc.category().name());
}

} // namespace
#endif//__fon9_ErrC_hpp__
