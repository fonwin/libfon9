// \file fon9/fmkt/FmktTools_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/FmktTools.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/Timer.hpp"

namespace f9fmkt = fon9::fmkt;
//--------------------------------------------------------------------------//
using FnMoveTicks = f9fmkt::Pri (*)(const f9fmkt::LvPriStep* steps, f9fmkt::Pri priFrom, uint16_t count, f9fmkt::Pri lmt);
// -----
#define TEST_DEF(fnName, priFrom, count, priLmt, expected)                                            \
   TestFn(f9fmkt::fnName, #fnName, static_cast<size_t>(__LINE__),                                     \
          f9fmkt::Pri{static_cast<double>(priFrom)}, count, f9fmkt::Pri{static_cast<double>(priLmt)}, \
          f9fmkt::Pri{static_cast<double>(expected)})
//--------------------------------------------------------------------------//
static f9fmkt::LvPriStepVect  lvSteps;
static const fon9::FmtDef     kPriFmt{9,3};
//--------------------------------------------------------------------------//
void TestFn(FnMoveTicks fnCall, fon9::StrView fnName, size_t lineNo,
            f9fmkt::Pri priFrom, uint16_t count, f9fmkt::Pri upLmt, f9fmkt::Pri expected) {
   printf("%s", fon9::RevPrintTo<std::string>(
      "[TEST ] @Ln ", lineNo, fon9::FmtDef{3},
      ":  ", fnName, "("
      "from=",  priFrom, kPriFmt,         ", "
      "count=", count,   fon9::FmtDef{4}, ", "
      "upLmt=", upLmt,   kPriFmt,         ") "
      "[expected=", expected, kPriFmt, "]"
      ).c_str());
   f9fmkt::Pri res = fnCall(lvSteps.Get(), priFrom, count, upLmt);
   if (res == expected) {
      puts("\r[OK   ]");
      return;
   }
   printf("%s", fon9::RevPrintTo<std::string>("  [result=", res, kPriFmt, "]").c_str());
   puts("\r[ERROR]");
   abort();
}
//--------------------------------------------------------------------------//
int main(int argc, char** argv) {
   (void)argc; (void)argv;
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   #endif

   fon9::AutoPrintTestInfo utinfo{"FmktTools"};
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{50});
   lvSteps.FromStr("0.5");
   printf("Ticks = %s\n", f9fmkt::LvPriStep_ToStr(lvSteps.Get()).begin());
   TEST_DEF(MoveTicksUp, +0.0, +5, +10,  +0.0 + 0.5 * 5);
   TEST_DEF(MoveTicksUp, +0.1, +5, +10,  +0.0 + 0.5 * 5);
   TEST_DEF(MoveTicksUp, +3.0, +5, +10,  +3.0 + 0.5 * 5);
   TEST_DEF(MoveTicksUp, +3.1, +5, +10,  +3.0 + 0.5 * 5);

   TEST_DEF(MoveTicksUp, -0.1, +5, +10,  -0.5 + 0.5 * 5);
   TEST_DEF(MoveTicksUp, -3.0, +5, +10,  -3.0 + 0.5 * 5);
   TEST_DEF(MoveTicksUp, -3.1, +5, +10,  -3.5 + 0.5 * 5);
   TEST_DEF(MoveTicksUp, +0.0, 50, +10,  10.0);
   //------------------------------------------//
   TEST_DEF(MoveTicksDn, +0.0, +5, -10,  +0.0 - 0.5 * 5);
   TEST_DEF(MoveTicksDn, +0.1, +5, -10,  +0.5 - 0.5 * 5);
   TEST_DEF(MoveTicksDn, +3.0, +5, -10,  +3.0 - 0.5 * 5);
   TEST_DEF(MoveTicksDn, +3.1, +5, -10,  +3.5 - 0.5 * 5);

   TEST_DEF(MoveTicksDn, -0.1, +5, -10,  -0.0 - 0.5 * 5);
   TEST_DEF(MoveTicksDn, -3.0, +5, -10,  -3.0 - 0.5 * 5);
   TEST_DEF(MoveTicksDn, -3.1, +5, -10,  -3.0 - 0.5 * 5);
   TEST_DEF(MoveTicksDn, +0.0, 50, -10, -10.0);
   //----------------------------------------------------------//
   utinfo.PrintSplitter();
   lvSteps.FromStr("10=0.01|50=0.05|100=0.1|500=0.5|1000=1|5");
   printf("Ticks = %s\n", f9fmkt::LvPriStep_ToStr(lvSteps.Get()).begin());
   TEST_DEF(MoveTicksUp,   0.000,    5, +3005,  +0.0 + 0.01 * 5);
   TEST_DEF(MoveTicksUp,   0.000, 1000, +3005,  +0.0 + 0.01 * 1000);
   TEST_DEF(MoveTicksUp,   0.000, 1100, +3005,  +0.0 + 0.01 * 1000 + 0.05 * 100);
   TEST_DEF(MoveTicksUp,   3.001,    5, +3005,  +3.0 + 0.01 * 5);
   TEST_DEF(MoveTicksUp,  10.000,    5, +3005,  10.0 + 0.05 * 5);
   TEST_DEF(MoveTicksUp,  15.000,    5, +3005,  15.0 + 0.05 * 5);
   TEST_DEF(MoveTicksUp,  15.000, 1000, +3005,  15.0 + 0.05 * 700 + 0.1 * 300);
   TEST_DEF(MoveTicksUp,  15.040, 1100, +3005,  15.0 + 0.05 * 700 + 0.1 * 400);
   TEST_DEF(MoveTicksUp,  55.000,    5, +3005,  55.0 + 0.10 * 5);
   TEST_DEF(MoveTicksUp,  55.000, 1000, +3005,  55.0 + 0.10 * 450 + 0.5 * 550);
   TEST_DEF(MoveTicksUp,  55.000, 2000, +3005,  55.0 + 0.10 * 450 + 0.5 * 800 + 1 * 500 + 5 * 250);

   TEST_DEF(MoveTicksUp,  -3.001,    5, +3005,  -3.01 + 0.01 * 5);
   TEST_DEF(MoveTicksUp, -10.000,    5, +3005, -10.00 + 0.01 * 5);
   TEST_DEF(MoveTicksUp, -15.000,    5, +3005, -15.00 + 0.05 * 5);
   TEST_DEF(MoveTicksUp, -15.000, 1000, +3005, -15.00 + 0.05 * 100 + 0.01 * 900);
   TEST_DEF(MoveTicksUp, -15.040, 1100, +3005, -15.05 + 0.05 * 101 + 0.01 * 999);
   TEST_DEF(MoveTicksUp, -55.0,      5, +3005, -55.00 + 0.10 * 5);
   TEST_DEF(MoveTicksUp, -55.0,   1000, +3005, -55.00 + 0.10 * 50 + 0.05 * 800 + 0.01 * 150);
   TEST_DEF(MoveTicksUp, -55.0,   2000, +3005, -55.00 + 0.10 * 50 + 0.05 * 800 + 0.01 * 1000 + 0.01 * 150);
   TEST_DEF(MoveTicksUp, -55.0,   2000,    -5,  -5.00);
   TEST_DEF(MoveTicksUp, -55.0,   2000,     1,   1.00);
   //------------------------------------------//
   TEST_DEF(MoveTicksDn,   0.000,    5, -3005,  +0.00 - 0.01 * 5);
   TEST_DEF(MoveTicksDn,   0.000, 1000, -3005,  +0.00 - 0.01 * 1000);
   TEST_DEF(MoveTicksDn,   0.000, 1100, -3005,  +0.00 - 0.01 * 1000 - 0.05 * 100);
   TEST_DEF(MoveTicksDn,   3.001,    5, -3005,  +3.01 - 0.01 * 5);
   TEST_DEF(MoveTicksDn,  10.000,    5, -3005,  10.00 - 0.01 * 5);
   TEST_DEF(MoveTicksDn,  15.000,    5, -3005,  15.00 - 0.05 * 5);
   TEST_DEF(MoveTicksDn,  15.000, 1000, -3005,  15.00 - 0.05 * 100 - 0.01 * 900);
   TEST_DEF(MoveTicksDn,  15.040, 1100, -3005,  15.05 - 0.05 * 101 - 0.01 * 999);
   TEST_DEF(MoveTicksDn,  55.000,    5, -3005,  55.00 - 0.10 * 5);
   TEST_DEF(MoveTicksDn,  55.000, 1000, -3005,  55.00 - 0.10 * 50 - 0.05 * 800 - 0.01 * 150);
   TEST_DEF(MoveTicksDn,  55.000, 2000, -3005,  55.00 - 0.10 * 50 - 0.05 * 800 - 0.01 * 1000 - 0.01 * 150);

   TEST_DEF(MoveTicksDn,  -3.001,    5, -3005,  -3.00 - 0.01 * 5);
   TEST_DEF(MoveTicksDn, -10.000,    5, -3005, -10.00 - 0.05 * 5);
   TEST_DEF(MoveTicksDn, -15.000,    5, -3005, -15.00 - 0.05 * 5);
   TEST_DEF(MoveTicksDn, -15.000, 1000, -3005, -15.00 - 0.05 * 700 - 0.1 * 300);
   TEST_DEF(MoveTicksDn, -15.040, 1100, -3005, -15.00 - 0.05 * 700 - 0.1 * 400);
   TEST_DEF(MoveTicksDn, -55.0,      5, -3005, -55.00 - 0.10 * 5);
   TEST_DEF(MoveTicksDn, -55.0,   1000, -3005, -55.00 - 0.10 * 450 - 0.5 * 550);
   TEST_DEF(MoveTicksDn, -55.0,   2000, -3005, -55.00 - 0.10 * 450 - 0.5 * 800 - 1 * 500 - 5 * 250);
   TEST_DEF(MoveTicksDn, -55.0,   2000,    -5,  -5.00);
   TEST_DEF(MoveTicksDn, -55.0,   2000, -1000, -1000.0);
   //----------------------------------------------------------//
   return 0;
}
