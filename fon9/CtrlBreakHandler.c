// \file fon9/CtrlBreakHandler.c
// \author fonwinz@gmail.com
#include "fon9/CtrlBreakHandler.h"
#include "fon9/ConsoleIO.h"

fon9_API const char* fon9_AppBreakMsg;

#ifdef fon9_WINDOWS
static DWORD fon9_MainThreadId;
fon9_API void fon9_CAPI_CALL fon9_BreakApp(const char* appBreakMsg) {
   fon9_AppBreakMsg = appBreakMsg;
   //FILE* fstdin = nullptr;
   //freopen_s(&fstdin, "NUL:", "r", stdin); // 如果遇到 CTRL_CLOSE_EVENT(或其他?) 會卡死在這兒!?
   //所以改用 CancelIoEx() 一樣可以強制中斷 gets();
   CancelIoEx(GetStdHandle(STD_INPUT_HANDLE), NULL);
}
static BOOL WINAPI fon9_WindowsCtrlBreakHandler(DWORD dwCtrlType) {
   switch (dwCtrlType) {
   case CTRL_C_EVENT:         fon9_BreakApp("<Ctrl-C>");       break;
   case CTRL_BREAK_EVENT:     fon9_BreakApp("<Ctrl-Break>");   break;
   case CTRL_CLOSE_EVENT:     fon9_BreakApp("<Close>");        break;
   case CTRL_LOGOFF_EVENT:    fon9_BreakApp("<Logoff>");       break;
   case CTRL_SHUTDOWN_EVENT:  fon9_BreakApp("<Shutdown>");     break;
   default:                   fon9_BreakApp("<Unknow Ctrl>");  break;
   }
   // 雖然 [MSDN](https://docs.microsoft.com/en-us/windows/console/handlerroutine)
   // 說: When the signal is received, the system creates a new thread in the process to execute the function.
   // 但這裡還是多此一舉的判斷一下 (this thread 不是 main thread 才要等 main thread 做完).
   if (fon9_MainThreadId != GetCurrentThreadId()) {
      do {
         CancelIoEx(GetStdHandle(STD_INPUT_HANDLE), NULL);
         Sleep(20);
      } while (fon9_AppBreakMsg);
      // 因為從這裡返回後 Windows 會直接呼叫 ExitProcess(); 造成 main thread 事情還沒做完.
      // 所以這裡直接結束 this thread, 讓 main thread 可以正常結束, 而不是 ExitProcess(); 強制結束程式.
      ExitThread(0);
   }
   return TRUE;
}
fon9_API void fon9_CAPI_CALL fon9_SetupCtrlBreakHandler(void) {
   fon9_MainThreadId = GetCurrentThreadId();
   SetConsoleCtrlHandler(&fon9_WindowsCtrlBreakHandler, TRUE);
}

#else
#include <signal.h>
fon9_API void fon9_CAPI_CALL fon9_BreakApp(const char* appBreakMsg) {
   fon9_AppBreakMsg = appBreakMsg;
   // 直接呼叫 freopen() 似乎無法讓 fgets() 中斷, 只有在 signal 時使用 freopen() 才有效?
   // 所以改成使用 kill() 方式結束程式.
   // if (!freopen("/dev/null", "r", stdin)) {
   // }
   kill(getpid(), SIGINT);
}
static void fon9_UnixSignalTermHandler(int sign) {
   if (fon9_AppBreakMsg == NULL)
      fon9_AppBreakMsg = "<Signal TERM>";
   if (!freopen("/dev/null", "r", stdin)) {
      // Linux 可透過 freopen() 中斷執行中的 fgets();
   }
}
fon9_API void fon9_CAPI_CALL fon9_SetupCtrlBreakHandler(void) {
   signal(SIGTERM, &fon9_UnixSignalTermHandler);
   signal(SIGINT, &fon9_UnixSignalTermHandler);
}
#endif
