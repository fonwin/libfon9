/// \file fon9/ConsoleIO.h
/// \author fonwinz@gmail.com
#ifndef __fon9_ConsoleIO_h__
#define __fon9_ConsoleIO_h__
#include "fon9/sys/Config.h"

#ifdef __cplusplus
extern "C" {
#endif

fon9_BEFORE_INCLUDE_STD
#ifdef fon9_WINDOWS
#  define _WINSOCKAPI_ // stops Windows.h including winsock.h
#  define NOMINMAX     // stops Windows.h #define max()
#  include <Windows.h>
#else
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif
#include <stdio.h>
fon9_AFTER_INCLUDE_STD

extern fon9_API int fon9_getch(void);

/// \ingroup Misc
/// 要求使用者從 console 輸入密碼.
/// buflen 包含 EOS'\0', 一般用法:
/// 如果 promptFD == NULL: 則不輸出 '*';
/// 如果 promptStr != NULL: 則: 若 promptFD==NULL 則使用 stderr 輸出;
/// \code
///   char pwstr[32]; // 最長 31 字元.
///   fon9_getpass(stdout, "Password: ", pwstr, sizeof(pwstr));
/// \endcode
/// \return 不含EOS的長度.
fon9_API size_t fon9_getpass(FILE* promptFD, const char* promptStr, char* pwbuf, size_t buflen);

#ifdef fon9_WINDOWS
typedef struct { int ws_row, ws_col; } fon9_winsize;
inline void fon9_GetConsoleSize(fon9_winsize* winsz) {
   CONSOLE_SCREEN_BUFFER_INFO csbi;
   GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
   winsz->ws_row = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
   winsz->ws_col = csbi.srWindow.Right - csbi.srWindow.Left + 1;
}
#else
typedef struct winsize  fon9_winsize;
inline void fon9_GetConsoleSize(fon9_winsize* winsz) {
   ioctl(STDOUT_FILENO, TIOCGWINSZ, winsz);
}
#endif

#ifdef __cplusplus
}
#endif
#endif//__fon9_ConsoleIO_h__
