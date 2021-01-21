// \file fon9/web/HtmlEncoder.cpp
// \author fonwinz@gmail.com
#include "fon9/web/HtmlEncoder.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace web {

fon9_API void RevEncodeHtml(RevBuffer& rbuf, StrView str) {
   const char* const pbeg = str.begin();
   for (const char* pend = str.end(); pend != pbeg;) {
      const char ch = *--pend;
      switch (ch) {
      case '&':   RevPrint(rbuf, "&amp;");   break;
      case '<':   RevPrint(rbuf, "&lt;");    break;
      case '>':   RevPrint(rbuf, "&gt;");    break;
      default:    RevPutChar(rbuf, ch);      break;
      }
   }
}

} } // namespace
