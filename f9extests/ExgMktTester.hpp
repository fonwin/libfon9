// \file f9extests/ExgMktTester.hpp
//
// 台灣證交所、OTC、期交所 行情測試共用模組.
//
// \author fonwinz@gmail.com
#ifndef __f9extests_ExgMktTester_hpp__
#define __f9extests_ExgMktTester_hpp__
#include "f9extests/SymbIn.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/buffer/DcQueue.hpp"

namespace f9extests {

inline unsigned long StrToVal(char* str) {
   unsigned long val = strtoul(str, &str, 10);
   switch (toupper(*str)) {
   case 'K': val *= 1024;  break;
   case 'M': val *= 1024 * 1024;  break;
   case 'G': val *= 1024 * 1024 * 1024;  break;
   }
   return val;
}

class f9extests_API MktDataFile {
   fon9_NON_COPY_NON_MOVE(MktDataFile);
public:
   const size_t   Size_;
   char* const    Buffer_;

   /// argv[0]=exe(not used), argv[1]=MarketDataFile, argv[2]=ReadFrom, argv[3]=ReadSize
   /// 一次全部載入到 this->Buffer_ 裡面,
   /// 因為 ExgMkt_UT 效率測試時, 要測試的是解析的時間, 避免讀檔造成評估效率的誤差.
   MktDataFile(char* argv[]);
   ~MktDataFile();
};

/// 將 mdf 一次全部餵給 pkReceiver.
/// 並顯示: 平均每筆封包處理時間. 封包數量、CheckSum錯誤次數, 錯誤的資料量.
template <class PkReceiverT>
double ExgMktFeedAll(const MktDataFile& mdf, PkReceiverT& pkReceiver) {
   fon9::DcQueueFixedMem   dcq{mdf.Buffer_, mdf.Size_};
   fon9::StopWatch         stopWatch;
   pkReceiver.FeedBuffer(dcq);
   const double tmFull = stopWatch.StopTimer();
   stopWatch.PrintResult(tmFull, "Fetch packet", pkReceiver.ReceivedCount_);

   std::cout << "ReceivedCount=" << pkReceiver.ReceivedCount_
      << "|ChkSumErrCount=" << pkReceiver.ChkSumErrCount_
      << "|DroppedBytes=" << pkReceiver.DroppedBytes_
      << std::endl;
   return tmFull;
}

/// step = 每次餵食的資料量, 用來測試「串流式」的資料來源.
template <class PkReceiverT>
void CheckExgMktFeederStep(const PkReceiverT& orig, const char* buf, size_t bufsz, size_t step) {
   std::cout << "[TEST ] step=" << step << std::flush;
   const char* const     bufend = buf + bufsz;
   const char*           pnext = buf;
   fon9::DcQueueFixedMem dcq{nullptr, nullptr};
   PkReceiverT           pkReceiver;
   while (pnext < bufend) {
      const char* cur = reinterpret_cast<const char*>(dcq.Peek1());
      if (cur == nullptr)
         cur = pnext;
      if ((pnext += step) >= bufend)
         pnext = bufend;
      dcq.Reset(cur, pnext);
      pkReceiver.FeedBuffer(dcq);
   }
   if (pkReceiver.ReceivedCount_ != orig.ReceivedCount_
       || pkReceiver.ChkSumErrCount_ != orig.ChkSumErrCount_
       || pkReceiver.DroppedBytes_ != orig.DroppedBytes_
       || memcmp(pkReceiver.FmtCount_, orig.FmtCount_, sizeof(orig.FmtCount_)) != 0
       || memcmp(pkReceiver.LastSeq_, orig.LastSeq_, sizeof(orig.LastSeq_)) != 0
       || memcmp(pkReceiver.SeqMisCount_, orig.SeqMisCount_, sizeof(orig.SeqMisCount_)) != 0) {
      std::cout << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;
}

/// 返回 step, step = 0 表示不測試此項目.
/// 檢查使用每次餵食 step bytes 的結果, 是否與 orig 的結果相同.
template <class PkReceiverT>
unsigned long CheckExgMktFeederStep(const PkReceiverT& orig, const MktDataFile& mdf, char* cstrStep) {
   if (cstrStep[0] == '?')
      return 0;
   auto step = StrToVal(cstrStep);
   if (step == 0)
      return 0;
   fon9::StopWatch stopWatch;
   CheckExgMktFeederStep(orig, mdf.Buffer_, mdf.Size_, step);
   stopWatch.PrintResult("Fetch packet by step", orig.ReceivedCount_);
   return step;
}

/// 測試 Parser 的速度及結果.
/// - struct ParserT : public PkReceiverT, public fon9::fmkt::SymbTree;
/// - 若 argv 有 "?SymbId" or "SymbId, 則顯示 SymbId 的最後成交及最後買賣報價.
/// - 若 argv 有 "?" 則使用 stdin 輸入 SymbId.
/// - 必須提供 ParserT::GetParsedCount(); 及 tmFull(=ExgMktFeedAll(mdf, pkReceiver)):
///   - 用來計算解析的速度, 但實際應用時, 還有序號連續性問題, 所以要花更久的時間.
//      TWS 測試結果:
//      Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
//      OS: Ubuntu 16.04.2 LTS
//      Parse Fmt6+17: 2.673751904 secs / 17,034,879 times = 156.957493153 ns
template <class ParserT>
void ExgMktTestParser(const char* msgHead, const MktDataFile& mdf, int argc, char** argv, double tmFull,
                      fon9::FmtDef fmtPri = fon9::FmtDef{7,2}) {
   fon9::DcQueueFixedMem    dcq{mdf.Buffer_, mdf.Size_};
   std::unique_ptr<ParserT> parserSP{new ParserT};
   ParserT&                 parser = *parserSP;
   fon9::StopWatch          stopWatch;
   parser.FeedBuffer(dcq);
   const double tmParsed = stopWatch.StopTimer();
   stopWatch.PrintResult(tmParsed - tmFull, msgHead, parser.GetParsedCount());
   std::cout << "Symbol count=" << parser.SymbMap_.Lock()->size() << std::endl;
   parser.PrintInfo();
   fon9::StrView  symbid;
   int            qidx = 0;
   bool           isInputSymb = false;
   std::string    lastQryId;
   for (;;) {
      if (qidx >= argc) {
         if (!isInputSymb)
            break;
         printf("Query symbol last state(q=quit, else=SymbId):");
         char  rdbuf[128];
         if (!fgets(rdbuf, static_cast<int>(sizeof(rdbuf)), stdin))
            break;
         symbid = fon9::StrView_cstr(rdbuf);
         if (fon9::StrTrim(&symbid).empty()) {
            if (lastQryId.empty())
               continue;
            symbid = "?";
         }
         if (symbid == "?") {
            auto symbs = parser.SymbMap_.Lock();
            auto ilast = lastQryId.empty() ? symbs->begin() : symbs->find(&lastQryId);
            if (!lastQryId.empty()) {
               if (ilast != symbs->end())
                  ++ilast;
            }
            if (ilast == symbs->end()) {
               printf("Iterator is end, last SymbId=[%s]\n", lastQryId.c_str());
               lastQryId.clear();
               continue;
            }
            lastQryId = ilast->first.ToString();
            symbid = &lastQryId;
         }
         else if (symbid == "q")
            break;
      }
      else {
         const char* pid = argv[qidx++];
         if (*pid == '?') {
            if (*++pid == '\0') {
               isInputSymb = true;
               continue;
            }
         }
         symbid = fon9::StrView_cstr(pid);
      }
      auto          symb = parser.GetSymb(symbid);
      const SymbIn* symi = static_cast<const SymbIn*>(symb.get());
      if (symi == nullptr) {
         printf("Symbol not found: [%s]\n", symbid.ToString().c_str());
         continue;
      }
      fon9::RevBufferList  rbuf{256};
      fon9::FmtDef         fmtQty{7};
      if (symi->BS_.Data_.DerivedBuy_.Qty_ || symi->BS_.Data_.DerivedSell_.Qty_) {
         RevPrint(rbuf, "--- Derived Sell & Buy:\n",
                  symi->BS_.Data_.DerivedSell_.Pri_, fmtPri, " / ", symi->BS_.Data_.DerivedSell_.Qty_, fmtQty, '\n',
                  symi->BS_.Data_.DerivedBuy_.Pri_, fmtPri, " / ", symi->BS_.Data_.DerivedBuy_.Qty_, fmtQty, '\n');
      }
      for (int L = fon9::fmkt::SymbBSData::kBSCount; L > 0;) {
         --L;
         RevPrint(rbuf, symi->BS_.Data_.Buys_[L].Pri_, fmtPri, " / ", symi->BS_.Data_.Buys_[L].Qty_, fmtQty, '\n');
      }
      RevPrint(rbuf, "---\n");
      for (int L = 0; L < fon9::fmkt::SymbBSData::kBSCount; ++L) {
         RevPrint(rbuf, symi->BS_.Data_.Sells_[L].Pri_, fmtPri, " / ", symi->BS_.Data_.Sells_[L].Qty_, fmtQty, '\n');
      }
      fon9::RevPrint(rbuf,
                     "SymbId=", symi->SymbId_, "\n"
                     "Last deal: ", symi->Deal_.Data_.DealTime(), "\n"
                     "      Pri: ", symi->Deal_.Data_.Deal_.Pri_, fmtPri, "\n"
                     "      Qty: ", symi->Deal_.Data_.Deal_.Qty_, fmtQty, "\n"
                     " TotalQty: ", symi->Deal_.Data_.TotalQty_, fmtQty, "\n"
                     "Last Sells --- Buys: ", symi->BS_.Data_.InfoTime_, "\n"
      );
      std::cout << fon9::BufferTo<std::string>(rbuf.MoveOut()) << std::endl;
   }
}

} // namespaces
#endif//__f9extests_ExgMktTester_hpp__
