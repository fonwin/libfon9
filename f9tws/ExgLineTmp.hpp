// \file f9tws/ExgLineTmp.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgLineTmp_hpp__
#define __f9tws_ExgLineTmp_hpp__
#include "f9tws/ExgLineArgs.hpp"
#include "fon9/io/Session.hpp"
#include "fon9/LogFile.hpp"

namespace f9tws {

class f9tws_API ExgTradingLineMgr;

fon9_WARN_DISABLE_PADDING;
class f9tws_API ExgLineTmpArgs : public ExgLineArgs {
   using base = ExgLineArgs;
public:
   TwsApCode   ApCode_{};
   bool        IsNeedsLog_{};
   void Clear() {
      base::Clear();
      this->ApCode_ = TwsApCode{};
      this->IsNeedsLog_ = false;
   }
   std::string Verify() const {
      if (ApCode_ == TwsApCode{})
         return "Unknown ApCode";
      return base::Verify();
   }
   fon9::ConfigParser::Result OnTagValue(fon9::StrView tag, fon9::StrView& value);
};
/// - 不改變 args.Market_ 您必須自行處理.
/// - cfg = "BrkId=|PvcId=|Pass=|ApCode=|Log=Y";
/// - 每個欄位都必須提供.
/// - retval.empty() 成功, retval = 失敗訊息.
f9tws_API std::string ExgLineTmpArgsParser(ExgLineTmpArgs& args, fon9::StrView cfg);

//--------------------------------------------------------------------------//
#define f9tws_kSIZE_TMP_HEAD_SUBSYSNAME   2
#define f9tws_kSIZE_TMP_HEAD_FUNCODE      2
#define f9tws_kSIZE_TMP_HEAD_MSGTYPE      2
#define f9tws_kSIZE_TMP_HEAD_FUNCMSG      (f9tws_kSIZE_TMP_HEAD_FUNCODE + f9tws_kSIZE_TMP_HEAD_MSGTYPE)
#define f9tws_kSIZE_TMP_HEAD_P            (f9tws_kSIZE_TMP_HEAD_SUBSYSNAME + f9tws_kSIZE_TMP_HEAD_FUNCMSG)
#define f9tws_kSIZE_TMP_HEAD_TIME         6
#define f9tws_kSIZE_TMP_HEAD_STCODE       2
#define f9tws_kSIZE_TMP_HEAD_A            (f9tws_kSIZE_TMP_HEAD_P + f9tws_kSIZE_TMP_HEAD_TIME \
                                             + f9tws_kSIZE_TMP_HEAD_STCODE)

using ExgTmpFuncMsgT = const char (*)[f9tws_kSIZE_TMP_HEAD_FUNCMSG + 1];

struct ExgTmpHead {
   fon9::CharAryF<f9tws_kSIZE_TMP_HEAD_SUBSYSNAME> SubsysName_;
   fon9::CharAryF<f9tws_kSIZE_TMP_HEAD_FUNCODE>    FuncCode_;
   fon9::CharAryF<f9tws_kSIZE_TMP_HEAD_MSGTYPE>    MsgType_;
   fon9::CharAryF<f9tws_kSIZE_TMP_HEAD_TIME>       Time_;
   fon9::CharAryF<f9tws_kSIZE_TMP_HEAD_STCODE>     StCode_;

   void SetFuncMsg(const char (&funcMsg)[f9tws_kSIZE_TMP_HEAD_FUNCMSG + 1]) {
      assert(funcMsg[f9tws_kSIZE_TMP_HEAD_FUNCMSG] == '\0');
      memcpy(&this->FuncCode_, funcMsg, sizeof(funcMsg) - 1);
   }
   void SetLocalTime(fon9::TimeStamp tm) {
      fon9::Pic9ToStrRev<f9tws_kSIZE_TMP_HEAD_TIME>(
         this->Time_.Chars_ + f9tws_kSIZE_TMP_HEAD_TIME,
         fon9::GetHHMMSS(tm));
   }
   void SetUtcTime(fon9::TimeStamp tm) {
      this->SetLocalTime(tm + fon9::GetLocalTimeZoneOffset());
   }
   void SetStCode(const char (&stCode)[f9tws_kSIZE_TMP_HEAD_STCODE + 1]) {
      assert(stCode[f9tws_kSIZE_TMP_HEAD_STCODE] == '\0');
      memcpy(&this->StCode_, stCode, sizeof(stCode) - 1);
   }

   uint8_t GetStCode() const {
      return fon9::Pic9StrTo<f9tws_kSIZE_TMP_HEAD_STCODE, uint8_t>(this->StCode_.Chars_);
   }
};
static_assert(sizeof(ExgTmpHead) == f9tws_kSIZE_TMP_HEAD_A, "struct ExgTmpHead; must pack?");

//--------------------------------------------------------------------------//
fon9_MSC_WARN_DISABLE(4265); /* class has virtual functions, but destructor is not virtual */
class f9tws_API ExgTmpIoRevBuffer : public fon9::RevBufferList {
   fon9_NON_COPYABLE(ExgTmpIoRevBuffer);
   using base = fon9::RevBufferList;
public:
   ExgTmpIoRevBuffer(fon9::BufferNodeSize newAllocReserved)
      : base{newAllocReserved + 8} { // 8 = SLM-HEAD + TAIL;
      #define f9tws_kCSTR_SLM_TAIL  "\xef\xef"
      #define f9tws_kSIZE_SLM_TAIL  static_cast<unsigned>(sizeof(f9tws_kCSTR_SLM_TAIL)-1)
      fon9::RevPutMem(*this, f9tws_kCSTR_SLM_TAIL, f9tws_kSIZE_SLM_TAIL);
   }
};
fon9_MSC_WARN_POP;

/// TWSE/OTC 的 「SLM/TMP 連線協定」處理程序.
class f9tws_API ExgTmpIoSession : public fon9::io::Session {
   fon9_NON_COPY_NON_MOVE(ExgTmpIoSession);
   using base = fon9::io::Session;

   fon9::io::Device*          Dev_{};
   fon9::AsyncFileAppenderSP  Log_;
   fon9::TimeStamp            LastRxTime_; // 包含 TMP,SLM 的接收都算.
   fon9::TimeStamp            LastTxTime_; // 包含 TMP,SLM 的送出都算.
   fon9::TimeStamp            TmpTxTime_;
   enum class SlmTmpSt {
      DevClose,
      ApBroken,
      TmpRelink,
      ApReady,
   };
   SlmTmpSt St_{};
   void CheckApBroken(SlmTmpSt st);
   void AsyncClose(std::string cause);

   void OnExgTmp_SlmReady();
   void OnExgTmp_RecvTmp(const char* pkptr, unsigned pksz);
   void WriteLog(fon9::StrView hdrinfo, fon9::StrView data);
   void WriteLogLinkSt(const fon9::io::StateUpdatedArgs& e);

protected:
   void OnDevice_Initialized(fon9::io::Device& dev) override;
   void OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) override;
   void OnDevice_StateUpdated(fon9::io::Device& dev, const fon9::io::StateUpdatedArgs& e) override;
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device& dev) override;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) override;
   void OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) override;

   virtual void OnExgTmp_ApReady() = 0;
   /// 只有在 ApReady => 不是Ready 時; 才會通知一次.
   virtual void OnExgTmp_ApBroken() = 0;
   virtual void OnExgTmp_ApPacket(const ExgTmpHead& pktmp, unsigned pksz) = 0;

   /// - tiNext = 如果沒送任何資料, 下次處發此事件的時間.
   /// - 返回值: 期待下次觸發的時間, retval.IsNullOrZero() 表示 AP 層不考慮計時(例:成交回報).
   /// - 預設, 如果 this->ApCfmLink_.FuncCode_ 不正確:
   ///   - do nothing;
   /// - 預設, 如果 this->ApCfmLink_.FuncCode_ 正確:
   ///   - 距離上次 (now - this->TmpTxTime()) + tiNext > 60秒, 送出 CfmLink;
   ///   - 返回: 期待下次觸發的時間;
   ///   - 衍生者可能要覆寫, 例: 下單線路, 如果塞單, 就不能送 AP-HB;
   virtual void OnExgTmp_CheckApTimer(fon9::TimeStamp now, fon9::TimeInterval tiNext);
   /// 最後一次傳送 TMP 的時間.
   fon9::TimeStamp TmpTxTime() const {
      return this->TmpTxTime_;
   }

public:
   ExgTradingLineMgr&      LineMgr_;
   const ExgLineTmpArgs    LineArgs_;

   // TWSE="10"; OTC="91";
   const fon9::CharAryF<f9tws_kSIZE_TMP_HEAD_SUBSYSNAME>  LinkSubName_;
   const fon9::CharAryF<f9tws_kSIZE_TMP_HEAD_P>           ApCfmLink_;

   ExgTmpIoSession(ExgTradingLineMgr& lineMgr, const ExgLineTmpArgs& lineargs, std::string logFileName);

   static constexpr fon9::TimeInterval GetHbIntervalSLM() {
      return fon9::TimeInterval_Second(25);
   }
   /// 包含 TMP,SLM 的接收都算.
   fon9::TimeStamp LastRxTime() const {
      return this->LastRxTime_;
   }

   void SendTmp(fon9::TimeStamp now, ExgTmpIoRevBuffer&&);
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_ExgLineTmp_hpp__
