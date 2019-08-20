// \file fon9/ConfigLoader_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/ConfigLoader.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/Timer.hpp"
#include "fon9/RevPrint.hpp"
#include <memory>

#ifdef fon9_POSIX
#include <unistd.h>
#include <sys/stat.h>
inline int _rmdir(const char* path) {
   return rmdir(path);
}
#else
#include <direct.h>//_rmdir()
#endif

//--------------------------------------------------------------------------//

#define kCSTR_DefaultConfigDir "testcfg"

using ConfigLoaderSP = std::unique_ptr<fon9::ConfigLoader>;
using LineCount = fon9::ConfigLoader::LineCount;
using Result = fon9::Outcome<LineCount>;

ConfigLoaderSP Test_CheckLoad(const char* testItem, const std::string& init, Result exp) {
   ConfigLoaderSP cfgld{new fon9::ConfigLoader{kCSTR_DefaultConfigDir}};
   Result         res;
   std::string    errmsg;
   try {
      res = Result{cfgld->CheckLoad(&init)};
   }
   catch (fon9::ConfigLoader::Err& e) {
      res = Result{e.ErrC_};
      errmsg = fon9::RevPrintTo<std::string>('\n', e);
   }
   fon9_CheckTestResult((std::string(testItem) + "\"" + fon9::StrView_ToEscapeStr(&init) + "\"" + errmsg).c_str(),
                        res == exp);
   return cfgld;
}
void Test_CheckVar(const std::string& init, fon9::StrView varName, fon9::StrView varCtx, LineCount lineCount) {
   auto cfgld = Test_CheckLoad("CheckLoad:", init, Result{lineCount});
   auto var = cfgld->GetVariable(varName);
   std::string varmsg = "Variable:   ";
   varName.AppendTo(varmsg);
   varmsg.append(" = ");
   if (var) {
      varmsg.push_back('"');
      varmsg += fon9::StrView_ToEscapeStr(&var->Value_.Str_);
      varmsg.push_back('"');
   }
   fon9_CheckTestResult(varmsg.c_str(), var && (varCtx == fon9::ToStrView(var->Value_.Str_)));
}

void TestConfigLoader() {
   #define kCSTR_SingleLineValue    "single line"
   Test_CheckVar("$var =   " kCSTR_SingleLineValue " \t  ",     "var", kCSTR_SingleLineValue, 1);

   #define kCSTR_MultiLineValue \
      "  line1\n"               \
      "line2    \n"             \
      "\n"                      \
      "   \n"                   \
      "  line5  \n"             \
      "  "
   Test_CheckVar("$var = {" kCSTR_MultiLineValue "}", "var", kCSTR_MultiLineValue, 6);

   #define kCSTR_MultiLineValueRM \
      "  line1# remark\n"       \
      "line2    # {\n"          \
      "# }\n"                   \
      "   \n"                   \
      "  line5  #}\n"           \
      "  "
   Test_CheckVar("$var = {" kCSTR_MultiLineValueRM "}", "var", kCSTR_MultiLineValue, 6);

   Test_CheckVar("$TxLang = ${TxLang:-zh}",                "TxLang", "zh", 1);
   Test_CheckVar("$TxLang = ${TxLang-zh}",                 "TxLang", "zh", 1);
   Test_CheckVar("$TxLang = {} $TxLang = ${TxLang:-zh}",   "TxLang", "zh", 1);
   Test_CheckVar("$TxLang = {} $TxLang = ${TxLang-zh}",    "TxLang", "",   1);
   Test_CheckVar("$TxLang = {en} $TxLang = ${TxLang:-zh}", "TxLang", "en", 1);
   Test_CheckVar("$TxLang = {en} $TxLang = ${TxLang-zh}",  "TxLang", "en", 1);

   Test_CheckLoad("BadMessage:", "$TxLang = {en", Result{std::errc::bad_message});
   Test_CheckLoad("BadMessage:", "$TxLang = {en} $TxLang = ${TxLang-zh", Result{std::errc::bad_message});
}

//--------------------------------------------------------------------------//

void WriteTestFile(std::string fn, fon9::StrView ctx) {
   fon9::File fd;
   auto       res = fd.Open(std::move(fn), fon9::FileMode::CreatePath | fon9::FileMode::Trunc | fon9::FileMode::Append);
   if (res) {
      res = fd.Append(ctx);
      if (res.HasResult() && res.GetResult() == ctx.size())
         return;
   }
   std::cout << "Open test file: " << fd.GetOpenName() << "\n"
      "err=" << fon9::RevPrintTo<std::string>(res) << std::endl;
   abort();
}

#define kCSTR_fn_recursive    "recursive.cfg"
#define kCSTR_fn_inWorkDir    "inWorkDir.cfg"

void RemoveTestFiles() {
   remove(kCSTR_fn_recursive);
   remove(kCSTR_fn_inWorkDir);
   _rmdir(kCSTR_DefaultConfigDir);
}

void TestConfigLoaderFile() {
   Test_CheckLoad("FileNotFound:", "nofile", Result{std::errc::no_such_file_or_directory});
   //-------------------------------
   WriteTestFile(kCSTR_fn_inWorkDir,
                 "$incfn={" kCSTR_fn_recursive "} "
                 "$include: $incfn");
   WriteTestFile(kCSTR_fn_recursive, "\n\n    $include:" kCSTR_fn_recursive); // Ln:3,Col:5 = $include:
   Test_CheckLoad("IncludeTooDeeply:", kCSTR_fn_inWorkDir, Result{std::errc::too_many_files_open});
   //-------------------------------
   // LineNo, ColNo 是否正確?
   WriteTestFile(kCSTR_fn_inWorkDir,
                 "Test.Line1\n"
                 "Test.Line2\n"
                 "Test.Line3\n"
                 "$include:" kCSTR_fn_recursive "\n"
                 "Test.Line7");
   WriteTestFile(kCSTR_fn_recursive,
                 "Test.Line4\n"
                 "Test.Line5\n"
                 "Test.Line6\n");
   const unsigned lnInclFrom = 4, lnInclTo = 6;
   auto cfgld = Test_CheckLoad("Line&Col:", kCSTR_fn_inWorkDir, Result{5});
   auto cfgstr = cfgld->GetCfgStr().c_str();
   unsigned ln = 1, col = 1;
   std::cout << "[TEST ] Check: Line#, Col#";
   while (*cfgstr) {
      auto from = cfgld->GetLineFrom(cfgstr);
      if (!from) {
         std::cout << "|err=No line from info|pos=" << (cfgstr - cfgld->GetCfgStr().c_str())
            << "\r[ERROR]" << std::endl;
         abort();
      }
      const bool  isIncl = (lnInclFrom <= ln && ln <= lnInclTo);
      const char* fn = isIncl ? kCSTR_fn_recursive : kCSTR_fn_inWorkDir;
      if (from->FileName_ != fn) {
         std::cout << "|ln=" << ln << "|from.fn=" << from->FileName_ <<
            "|err=File name not expected"
            "|pos=" << (cfgstr - cfgld->GetCfgStr().c_str()) << "\r[ERROR]" << std::endl;
         abort();
      }
      unsigned fnln = ln;
      if (lnInclFrom <= ln) {
         if (ln <= lnInclTo)
            fnln -= (lnInclFrom - 1);
         else {
            fnln -= (lnInclTo - lnInclFrom + 1);
            ++fnln;
         }
      }
      if (from->FileLn_ != fnln) {
         std::cout << "|ln=" << ln <<
            "|from.FileLn=" << from->FileLn_ << "|fnln=" << fnln <<
            "|err=FileLn not expected"
            "|pos=" << (cfgstr - cfgld->GetCfgStr().c_str()) << "\r[ERROR]" << std::endl;
         abort();
      }
      if (from->ColNo_ != col) {
         std::cout << "|ln=" << ln <<
            "|from.ColNo=" << from->ColNo_ << "|col=" << col <<
            "|pos=" << (cfgstr - cfgld->GetCfgStr().c_str()) << "\r[ERROR]" << std::endl;
         abort();
      }
      ++col;
      if (*cfgstr == '\n') {
         ++ln;
         col = 1;
      }
      ++cfgstr;
   }
   std::cout << "\r[OK   ]" << std::endl;
   //-------------------------------
   // TODO: $include: 搜尋路徑是否正確?
   // TODO: 展開後的 GetCfgStr() 是否正確?
}

//--------------------------------------------------------------------------//

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo{"ConfigLoader"};
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});
   RemoveTestFiles();

   TestConfigLoader();
   TestConfigLoaderFile();

   RemoveTestFiles();
}
