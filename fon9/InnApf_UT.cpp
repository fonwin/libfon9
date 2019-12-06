// \file fon9/InnApf_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/Timer.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/InnApf.hpp"
#include "fon9/RevPrint.hpp"
//--------------------------------------------------------------------------//
static const char                kInnApfFileName[] = "./InnApf_UT.f9apf";
constexpr fon9::InnFile::SizeT   kBlockSize = 32;
//--------------------------------------------------------------------------//
using OpenResult = fon9::InnApf::OpenResult;
void CheckOpenResult(OpenResult ores, OpenResult expected) {
   if (ores == expected)
      return;
   std::cout << fon9::RevPrintTo<std::string>("|OpenResult=", ores, "|expected=", expected)
      << "\r[ERROR]" << std::endl;
   abort();
}

using FnTestStream = std::function<void(unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw)>;

void TestInnApfRW(unsigned innOpenResult, unsigned keyCount, fon9::FileMode streamMode, FnTestStream fnTest) {
   fon9::InnStream::OpenArgs  streamArgs{fon9::InnRoomType{}};
   streamArgs.ExpectedRoomSize_[0] = 64;

   fon9::InnApf::OpenResult   ores;
   fon9::InnApf::OpenArgs     innArgs{kInnApfFileName, kBlockSize};
   fon9::StopWatch            stopWatch;

   std::cout << "|Apf Open:";
   stopWatch.ResetTimer();
   fon9::InnApfSP apf = fon9::InnApf::Make(innArgs, streamArgs, ores);
   std::cout << fon9::AutoTimeUnit{stopWatch.StopTimer()};
   CheckOpenResult(ores, OpenResult{innOpenResult});

   std::cout << "|Stream RW:";
   stopWatch.ResetTimer();
   fon9::InnApf::StreamRW  aprw;
   fon9::NumOutBuf         nbuf;
   for (unsigned L = 0; L < keyCount; ++L) {
      fon9::StrView keystr{fon9::ToStrRev(nbuf.end(), L), nbuf.end()};
      ores = aprw.Open(*apf, keystr, streamMode);
      fnTest(L, ores, aprw);
   }
   std::cout << fon9::AutoTimeUnit{stopWatch.StopTimer()};

   std::cout << "|Close:";
   stopWatch.ResetTimer();
   aprw.Close();
   apf.reset();
   std::cout << fon9::AutoTimeUnit{stopWatch.StopTimer()};

   std::cout << "\r[OK   ]" << std::endl;
}
void TestInnStreamRead(fon9::InnApf::StreamRW& aprw, const std::string& resExpected, uint64_t rdFrom = 0) {
   std::string rdin;
   rdin.reserve(resExpected.size() + 1);
   for (char ch : resExpected)
      rdin.push_back(static_cast<char>(~ch));
   rdin.push_back('\xff'); // 讀取要求 size > stream size;
   auto rdsz = aprw.Read(rdFrom, &*rdin.begin(), rdin.size());
   bool isRdEqual = (0 == memcmp(rdin.c_str(), resExpected.c_str(), rdsz));
   if (rdsz == resExpected.size()) {
      if (isRdEqual)
         return;
   }
   else {
      std::cout << "|rdsz=" << rdsz << "|expected=" << resExpected.size();
   }

   std::cout << "|streamKey=" << aprw.GetKey().ToString()
      << "|rdFrom=" << rdFrom
      << "|err=Read not expected."
      << (isRdEqual ? "" : " ctx not equal.")
      << "\r[ERROR]" << std::endl;
   abort();
}
void TestInnStreamWrite(fon9::InnApf::StreamRW& aprw, const std::string& resExpected) {
   auto wrsz = aprw.Write(0, fon9::DcQueueFixedMem(resExpected));
   if (wrsz == resExpected.size()) {
      TestInnStreamRead(aprw, resExpected);
      return;
   }
   std::cout << "|streamKey=" << aprw.GetKey().ToString()
      << "|expectedSize=" << resExpected.size() << "|result=" << wrsz
      << "|err=Write result not expected."
      << "\r[ERROR]" << std::endl;
   abort();
}
void TestInnStreamAppendNoBuffer(fon9::InnApf::StreamRW& aprw, const std::string& resExpected, uint64_t szExpected) {
   auto streamSize = aprw.AppendNoBuffer(resExpected.c_str(), resExpected.size());
   if (streamSize == szExpected) {
      TestInnStreamRead(aprw, resExpected, streamSize - resExpected.size());
      return;
   }
   std::cout << "|streamKey=" << aprw.GetKey().ToString()
      << "|expectedSize=" << szExpected << "|result=" << streamSize
      << "|err=AppendNoBuffer result not expected."
      << "\r[ERROR]" << std::endl;
   abort();
}
void TestInnStreamAppendBuffered(fon9::InnApf::StreamRW& aprw, std::string& curr, unsigned idx, bool isFromEmptyStream) {
   fon9::BufferList buf;
   // 當 curr 資料量大時, 可以測試使用 2 個 buffer nodes 的情況.
   fon9::AppendToBuffer(buf, &idx, sizeof(idx));
   fon9::AppendToBuffer(buf, curr.c_str(), curr.size());
   // 更新 curr = stream 的內容.
   if (isFromEmptyStream)
      curr.clear();
   fon9::BufferAppendTo(buf, curr);
   // 使用 AppendBuffered() 方式寫入 stream;
   aprw.AppendBuffered(std::move(buf));
}

void TestInnApf() {
   const unsigned kKeyCount = 1000;
   std::cout << "KeyCount=" << kKeyCount << std::endl;

   // 測試 open stream, 不寫: 不分配空間.
   remove(kInnApfFileName);
   std::cout << "[TEST ] Read(Empty):   ";
   TestInnApfRW(0, kKeyCount, fon9::FileMode::Read | fon9::FileMode::OpenAlways,
                [](unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw) {
      (void)idx; (void)ores;
      // 測試前有 remove(kInnApfFileName); 所以必定是新檔, 所有的 stream 都是新建, 沒有任何資料.
      TestInnStreamRead(aprw, std::string{});
   });

   // 建立測試資料.
   std::string testData[kKeyCount];
   char        ch = '\0';
   for (unsigned L = 1; L < kKeyCount; ++L) {
      std::string& str = testData[L];
      str.reserve(L);
      for (unsigned i = 0; i < L; ++i)
         str.push_back(++ch);
   }

   // 測試重新開啟 apf: 載入 stream info(全都未分配); 寫入單數 stream;
   std::cout << "[TEST ] Write(From 0): ";
   TestInnApfRW(kKeyCount, kKeyCount, fon9::FileMode::Write,
                [&testData](unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw) {
      (void)ores;
      TestInnStreamRead(aprw, std::string{});
      if (idx % 2 == 1)
         TestInnStreamWrite(aprw, testData[idx]);
   });
   // 測試重新開啟 apf: 寫入偶數 stream;
   std::cout << "[TEST ] AppendNoBuffer:";
   TestInnApfRW(kKeyCount, kKeyCount, fon9::FileMode::Append,
                [&testData](unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw) {
      (void)ores;
      if (idx % 2 == 1) {
         TestInnStreamRead(aprw, testData[idx]);
         TestInnStreamAppendNoBuffer(aprw, testData[idx - 1], testData[idx - 1].size() + testData[idx].size());
         testData[idx].append(testData[idx - 1]);
      }
      else {
         TestInnStreamRead(aprw, std::string{});
         TestInnStreamWrite(aprw, testData[idx]);
      }
   });

   std::cout << "[TEST ] AppendBuffered:";
   TestInnApfRW(kKeyCount, kKeyCount, fon9::FileMode::Append,
                [&testData](unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw) {
      (void)ores;
      TestInnStreamAppendBuffered(aprw, testData[idx], idx, false);
   });

   // 測試重新開啟 apf 檢查寫入內容;
   std::cout << "[TEST ] Read(CheckAll):";
   TestInnApfRW(kKeyCount, kKeyCount, fon9::FileMode::Read,
                [&testData](unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw) {
      (void)ores;
      TestInnStreamRead(aprw, testData[idx]);
   });

   //測試: 當 stream empty 時 AppendBuffered();
   remove(kInnApfFileName);
   std::cout << "[TEST ] AppendBuffered(Empty):";
   TestInnApfRW(0, kKeyCount, fon9::FileMode::CreatePath,
                [&testData](unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw) {
      (void)ores;
      TestInnStreamAppendBuffered(aprw, testData[idx], idx, true);
   });
   std::cout << "[TEST ] Read(CheckAll):";
   TestInnApfRW(kKeyCount, kKeyCount, fon9::FileMode::Read,
                [&testData](unsigned idx, OpenResult ores, fon9::InnApf::StreamRW& aprw) {
      (void)ores;
      TestInnStreamRead(aprw, testData[idx]);
   });
}
//--------------------------------------------------------------------------//
void InnApf_Benchmark(const double   kTestSpan) {
   uint64_t       dat = 0;
   const size_t   kKeyCount = 1000;
   const size_t   kRecordSize = sizeof(dat) * 16;
   std::cout << "Benchmark: "
      << "|KeyCount=" << kKeyCount
      << "|RecordSize=" << kRecordSize
      << "|TestSpan=" << kTestSpan
      << std::endl;
   fon9::InnStream::OpenArgs  streamArgs{fon9::InnRoomType{}};
   streamArgs.ExpectedRoomSize_[0] = kRecordSize * 100;

   const fon9::InnRoomSize    kApfBlockSize = streamArgs.ExpectedRoomSize_[0];
   fon9::InnApf::OpenArgs     innArgs{kInnApfFileName, kApfBlockSize};
   fon9::InnApf::OpenResult   ores;

   remove(kInnApfFileName);
   std::cout << "[TEST ] Open:          ";
   fon9::InnApfSP apf = fon9::InnApf::Make(innArgs, streamArgs, ores);
   CheckOpenResult(ores, OpenResult{0});

   struct KeyRec {
      fon9_NON_COPYABLE(KeyRec);
      KeyRec() = default;
      fon9::InnApf::StreamRW  Aprw_;
      std::string             Ctx_;
   };
   KeyRec*           testData = new KeyRec[kKeyCount];
   fon9::NumOutBuf   nbuf;
   fon9::StopWatch   stopWatch;
   for (size_t L = 0; L < kKeyCount; ++L) {
      fon9::StrView keystr{fon9::ToStrRev(nbuf.end(), L), nbuf.end()};
      ores = testData[L].Aprw_.Open(*apf, keystr, fon9::FileMode::CreatePath);
      CheckOpenResult(ores, OpenResult{0});
   }
   std::cout << fon9::AutoTimeUnit{stopWatch.StopTimer()} << "\r[OK   ]" << std::endl;
   for (size_t L = 0; L < kKeyCount; ++L)
      testData[L].Ctx_.reserve(kRecordSize * 10000);

   size_t totalRecCount = 0;
   double span;
   std::cout << "[TEST ] AppendBuffered:";
   stopWatch.ResetTimer();
   for (;;) {
      for (size_t LKey = 0; LKey < kKeyCount; ++LKey) {
         ++totalRecCount;
         KeyRec& rec = testData[LKey];
         auto    from = rec.Ctx_.size();
         for (size_t LDat = 0; LDat < kRecordSize / sizeof(dat); ++LDat)
            rec.Ctx_.append(reinterpret_cast<char*>(&++dat), sizeof(dat));
         rec.Aprw_.AppendBuffered(rec.Ctx_.c_str() + from, rec.Ctx_.size() - from, kApfBlockSize);
      }
      if ((span = stopWatch.CurrSpan()) >= kTestSpan)
         break;
   }
   std::cout << fon9::AutoTimeUnit{span}
      << " / " << totalRecCount
      << " recs = " << fon9::AutoTimeUnit{span / static_cast<double>(totalRecCount)}
      << "\r[OK   ]" << std::endl;

   // 讀取時會觸發 Flush, 將 AppendBuffered() 尚在 buffer 的資料寫入 InnFile.
   std::cout << "[TEST ] Flush & Check: ";
   stopWatch.ResetTimer();
   for (size_t LKey = 0; LKey < kKeyCount; ++LKey) {
      KeyRec& rec = testData[LKey];
      TestInnStreamRead(rec.Aprw_, rec.Ctx_);
   }
   std::cout << fon9::AutoTimeUnit{stopWatch.StopTimer()} << "\r[OK   ]" << std::endl;

   std::cout << "[TEST ] Close:         ";
   stopWatch.ResetTimer();
   delete[] testData;
   // 等候在 ThreadPool 裡面等候處理完畢.
   while (apf->use_count() != 1)
      std::this_thread::yield();
   apf.reset();
   std::cout << fon9::AutoTimeUnit{stopWatch.StopTimer()} << "\r[OK   ]" << std::endl;
}
//--------------------------------------------------------------------------//
int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo("InnApf");
   fon9::GetDefaultThreadPool();
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{100});

   TestInnApf();

   for (double L = 0.5; L <= 3; L += 0.5) {
      utinfo.PrintSplitter();
      InnApf_Benchmark(L);
   }
   remove(kInnApfFileName);
}
