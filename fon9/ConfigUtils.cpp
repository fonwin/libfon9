// \file fon9/ConfigUtils.cpp
// \author fonwinz@gmail.com
#include "fon9/ConfigUtils.hpp"

namespace fon9 {

fon9_API EnabledYN StrTo(StrView str, EnabledYN defaultValue, const char** endptr) {
   const char* pbeg = StrTrimHead(&str).begin();
   switch (toupper(str.Get1st())) {
   case 'Y':
      if (endptr) {
         ++pbeg;
         if (str.size() >= 3) { // "Yes"
            if (toupper(pbeg[0]) == 'E' && toupper(pbeg[1]) == 'S')
               pbeg += 2;
         }
         *endptr = pbeg;
      }
      return EnabledYN::Yes;
   case 'N':
      if (endptr) {
         ++pbeg;
         if (str.size() >= 2) { // "No"
            if (toupper(pbeg[0]) == 'O')
               ++pbeg;
         }
         *endptr = pbeg;
      }
      return EnabledYN{};
   case EOF:
      if (endptr)
         *endptr = pbeg;
      return EnabledYN{};
   default:
      if (endptr)
         *endptr = pbeg;
      return defaultValue;
   }
}

} // namespaces
