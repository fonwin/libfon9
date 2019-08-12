/// \file fon9/ConsoleIO.cpp
/// \author fonwinz@gmail.com
#include "fon9/ConsoleIO.h"
#include <string>
#include <ctype.h>

fon9_BEFORE_INCLUDE_STD
#ifdef fon9_WINDOWS
#  include <io.h>
#  include <conio.h> // _getch()
#else
#  include <unistd.h>
#  include <termios.h>
#  include <string.h>
#endif
fon9_AFTER_INCLUDE_STD

#ifdef __cplusplus
extern "C" {
#endif

#ifdef fon9_WINDOWS
fon9_API int fon9_getch() {
   return _isatty(_fileno(stdin)) ? _getch() : fgetc(stdin);
}
#else
fon9_API int fon9_getch() {
   struct termios cur;
   memset(&cur, 0, sizeof(cur));
   if (tcgetattr(STDIN_FILENO, &cur) < 0) {
      // perror("tcsetattr()"); // 不是 tty, 也許用 "< inputfile" 或 pipeline | 方式執行.
   }
   const struct termios old = cur;
   cfmakeraw(&cur);
   if (tcsetattr(STDIN_FILENO, TCSANOW, &cur) < 0) {
      // perror("tcsetattr TCSANOW");
   }
   int ch = getchar();
   if (tcsetattr(STDIN_FILENO, TCSANOW, &old) < 0) {
      // perror ("tcsetattr ~TCSANOW");
   }
   return ch;
}
#endif

fon9_API size_t fon9_getpass(FILE* promptFD, const char* promptStr, char* pwbuf, const size_t buflen) {
   if (promptStr && *promptStr)
      fprintf(promptFD ? promptFD : stderr, "%s", promptStr);
   size_t pos = 0;
   pwbuf[0] = 0;
   for (;;) {
      if (promptFD)
         fflush(promptFD);
      int ch = fon9_getch();
      switch (ch) {
      case '\b':
         if (pos > 0) {
            if (promptFD)
               fprintf(promptFD, "\b \b");
            pwbuf[--pos] = 0;
            continue;
         }
         break;
      default:
         if (0 < ch && isprint(static_cast<unsigned char>(ch))) {
            if (pos < buflen - 1) {
               pwbuf[pos++] = static_cast<char>(ch);
               pwbuf[pos] = 0;
               if (promptFD)
                  fputc('*', promptFD);
               continue;
            } // else = over buflen.
         } // else = control code.
         break;
      case EOF:
      case '\n':
      case '\r':
         // 為了安全, 清除 '*', 不要讓有心人士知道密碼長度;
         if (promptFD) {
            std::string bks(pos, '\b');
            std::string spcs(pos, ' ');
            fprintf(promptFD, "%s%s%s%s\n",
                    bks.c_str(), spcs.c_str(), bks.c_str(),
                   "------------------------------");
         }
         return pos;
      }
      if (promptFD)
         fputc('\a', promptFD);//beep.
   }
}

#ifdef __cplusplus
}
#endif
