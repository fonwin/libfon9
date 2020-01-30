// \file fon9/CTools.c
// \author fonwinz@gmail.com
#include "fon9/CTools.h"
#include <ctype.h>
#include <string.h>

fon9_API char* fon9_CAPI_CALL fon9_StrTrimHead(char* pbeg) {
   while (*pbeg != '\0' && isspace((unsigned char)*pbeg))
      ++pbeg;
   return pbeg;
}

fon9_API char* fon9_CAPI_CALL fon9_StrTrimTail(char* pbeg, char* pend) {
   while (pend != pbeg) {
      if (!isspace((unsigned char)(*--pend)))
         return pend + 1;
   }
   return pend;
}

fon9_API char* fon9_CAPI_CALL fon9_StrCutSpace(char* pbeg, char** pnext) {
   char* pspl = pbeg = fon9_StrTrimHead(pbeg);
   if (*pspl) {
      while (*++pspl) {
         if (isspace((unsigned char)*pspl)) {
            *pspl = '\0';
            *pnext = fon9_StrTrimHead(pspl + 1);
            return pbeg;
         }
      }
   }
   *pnext = NULL;
   return pbeg;
}

fon9_API const char* fon9_CAPI_CALL fon9_StrFetchNoTrim(const char* const pbeg,
                                                        const char** ppend,
                                                        const char* const delims) {
   const char* pspl = pbeg;
   while (*pspl) {
      if (strchr(delims, *pspl))
         break;
      ++pspl;
   }
   *ppend = pspl;
   return pbeg;
}
