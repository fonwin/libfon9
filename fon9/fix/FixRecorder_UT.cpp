﻿// \file fon9/fix/FixRecorder_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/fix/FixRecorder.hpp"
#include "fon9/fix/FixBuilder.hpp"
#include "fon9/Timer.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace f9fix = fon9::fix;

//--------------------------------------------------------------------------//
void BuildTestMessage(f9fix::FixBuilder& fixb, fon9::StrView headerCompIds, unsigned testn, f9fix::FixSeqNum seqNum) {
   #define f9fix_kMSGTYPE_NewOrderSingle  "D"
   fon9::RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(Text), "FixRecorderTest #", testn);
   fon9::RevPut_TimeFIXMS(fixb.GetBuffer(), fon9::UtcNow()); // SendingTime.
   fon9::RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(SendingTime));
   fon9::RevPrint(fixb.GetBuffer(), f9fix_SPLFLDMSGTYPE(NewOrderSingle) f9fix_SPLTAGEQ(MsgSeqNum), seqNum, headerCompIds);
}
void WriteFixBuilderFinalMsg(f9fix::FixRecorder& fixr, f9fix::FixBuilder& fixb) {
   f9fix::FixParser     fixpar;
   f9fix::FixRecvEvArgs rxargs{fixpar};
   std::string origFixMsg = fon9::BufferTo<std::string>(fixb.Final(ToStrView(fixr.BeginHeader_)));
   rxargs.SetOrigMsgStr(&origFixMsg);
   rxargs.SetRxTime(fon9::UtcNow());
   fixr.WriteInputConform(rxargs);
}
void TestFixRecorder(f9fix::FixRecorder& fixr, const unsigned kTimes) {
   for (unsigned L = 0; L < kTimes; ++L) {
      f9fix::FixBuilder fixb;
      // Test: record recv message.
      BuildTestMessage(fixb, ToStrView(fixr.CompIDs_.Header_), L + 1, fixr.GetNextRecvSeq());
      WriteFixBuilderFinalMsg(fixr, fixb);

      // Test: record send message.
      fixb.Restart();
      auto lk{fixr.Lock()};
      auto seq = fixr.GetNextSendSeq(lk);
      BuildTestMessage(fixb, ToStrView(fixr.CompIDs_.Header_), L + 1, seq);
      fon9::BufferList     sbuf = fixb.Final(ToStrView(fixr.BeginHeader_));
      fon9::RevBufferList  rbuf{1024};
      fon9::RevPrint(rbuf, f9fix_kCSTR_HdrSend, fon9::UtcNow(), ' ', sbuf, '\n');
      // send(sbuf);
      fixr.WriteAfterSend(std::move(lk), std::move(rbuf), ++seq);
      // unlock;
   }
}

void CheckFixMessage(fon9::StrView fixmsg, f9fix::FixSeqNum expectSeqNum) {
   f9fix::FixParser fixpr;
   auto             res = fixpr.Parse(fixmsg);
   if (res <= f9fix::FixParser::NeedsMore) {
      std::cout << "FIX Message Parse error:" << res << "|expectSeqNum=" << expectSeqNum <<  std::endl;
      abort();
   }
   if (fixpr.GetMsgSeqNum() != expectSeqNum) {
      std::cout << "Not expected FIX Message:expectSeqNum=" << expectSeqNum
                << "|currentSeqNum=" << fixpr.GetMsgSeqNum() << std::endl;
      abort();
   }
}
void CheckReloadSent(f9fix::FixRecorder& fixr, f9fix::FixSeqNum seqFrom, f9fix::FixSeqNum count) {
   f9fix::FixRecorder::ReloadSent reloader;
   CheckFixMessage(reloader.Start(fixr, seqFrom), seqFrom);
   while (count > 1) {
      CheckFixMessage(reloader.FindNext(fixr), ++seqFrom);
      --count;
   }
   if (!reloader.FindNext(fixr).empty()) {
      std::cout << "err=Last FindNext() is not empty." "\r" "[ERROR]" << std::endl;
      abort();
   }
}
//--------------------------------------------------------------------------//

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"FixRecorder"};
   fon9::GetDefaultTimerThread();
   fon9::GetDefaultThreadPool();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   f9fix::CompIDs       compIds{"SenderCoId", "SenderSubId", "TargetCoId", "TargetSubId"};
   f9fix::FixRecorderSP fixr{new f9fix::FixRecorder(f9fix_BEGIN_HEADER_V42, f9fix::CompIDs{compIds})};
   const char           fixrFileName[] = "FixRecorder_UT.log";
   remove(fixrFileName);
   auto res = fixr->Initialize(fixrFileName);
   if (!res) {
      std::cout << "Open FixRecorder|fileName=" << fixrFileName
         << "|err=" << fon9::RevPrintTo<std::string>(res) << std::endl;
      abort();
   }

   fon9::StopWatch   stopWatch;
   const unsigned    kTimes = 1000;

   stopWatch.ResetTimer();
   TestFixRecorder(*fixr, kTimes);
   stopWatch.PrintResult("FixRecorder(recv + send)", kTimes);

   std::cout << "[TEST ] Reload sent.";
   unsigned pers = 0;
   for (unsigned L = 0; L < kTimes; ++L) {
      CheckReloadSent(*fixr, L + 1, kTimes - L);
      unsigned p = (L + 1) * 100 / kTimes;
      if (pers != p) {
         pers = p;
         fprintf(stdout, "%3u%%\b\b\b\b", pers);
         fflush(stdout);
      }
   }
   std::cout << "\r" "[OK   ]" << std::endl;

   // Test: 檔案已存在, NextSendSeq, NextRecvSeq 是否正確.
   // 再寫入一筆 recv, 讓 NextRecvSeq != NextSendSeq
   f9fix::FixBuilder fixb;
   BuildTestMessage(fixb, ToStrView(fixr->CompIDs_.Header_), kTimes + 1, fixr->GetNextRecvSeq());
   WriteFixBuilderFinalMsg(*fixr, fixb);
   fixr->WaitFlushed();
   fixr.reset(new f9fix::FixRecorder(f9fix_BEGIN_HEADER_V42, f9fix::CompIDs{compIds}));
   int count = 100;
   do {
      if (--count <= 0) {
         std::cout << "Reopen FixRecorder|fileName=" << fixrFileName
            << "|err=" << fon9::RevPrintTo<std::string>(res) << std::endl;
         abort();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
   } while (!fixr->Initialize(fixrFileName));
   if (fixr->GetNextRecvSeq() != kTimes + 2 || fixr->GetNextSendSeq(fixr->Lock()) != kTimes + 1) {
      std::cout << "Reopen FixRecorder|fileName=" << fixrFileName
         << "|err=Unexpected NextSeq|expectNextRecvSeq=" << kTimes + 2
         << "|nextRecvSeq=" << fixr->GetNextRecvSeq()
         << "|expectNextSendSeq=" << kTimes + 1
         << "|nextSendSeq=" << fixr->GetNextSendSeq(fixr->Lock())
         << std::endl;
      abort();
   }

   // 結束前刪除測試檔.
   fixr.reset();
   if (!fon9::IsKeepTestFiles(argc, argv))
      fon9::WaitRemoveFile(fixrFileName);
}
