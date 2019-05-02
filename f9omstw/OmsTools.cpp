// \file f9omstw/OmsTools.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsTools.hpp"
#include "fon9/StrTo.hpp"

namespace f9omstw {

bool IncStrDec(char* pbeg, char* pend) {
   while (pbeg < pend) {
      if (fon9_LIKELY(*--pend < '9')) {
         assert('0' <= *pend);
         ++(*pend);
         return true;
      }
      *pend = '0';
   }
   return false;
}
bool IncStrAlpha(char* pbeg, char* pend) {
   while (pbeg < pend) {
      const char ch = *--pend;
      if (fon9_LIKELY(ch < '9')) {
         assert('0' <= ch);
         ++(*pend);
         return true;
      }
      if (fon9_UNLIKELY(ch == '9')) {
         *pend = 'A';
         return true;
      }
      if (fon9_LIKELY(ch < 'Z')) {
         assert('A' <= ch);
         ++(*pend);
         return true;
      }
      if (fon9_UNLIKELY(ch == 'Z')) {
         *pend = 'a';
         return true;
      }
      if (fon9_LIKELY(ch < 'z')) {
         assert('a' <= ch);
         ++(*pend);
         return true;
      }
      *pend = '0';
   }
   return false;
}

} // namespaces
