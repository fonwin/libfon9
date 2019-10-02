// \file f9twf/ExgLineTmpSession.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgLineTmpSession_hpp__
#define __f9twf_ExgLineTmpSession_hpp__
#include "f9twf/ExgLineTmpLog.hpp"
#include "fon9/io/Session.hpp"

namespace f9twf {

class f9twf_API ExgIoManager;
class f9twf_API ExgLineTmpSession;

class f9twf_API ExgLineTmpRevBuffer {
   fon9_NON_COPY_NON_MOVE(ExgLineTmpRevBuffer);
   using base = fon9::RevBufferList;
   friend class f9twf_API ExgLineTmpSession;
   fon9::RevBufferList  RBuf_;

public:
   ExgLineTmpRevBuffer() : RBuf_{0} {
   }

   /// 在 ExgLineTmpSession::SendTmp() 之前, 只能分配一個 TmpPacket.
   template <class TmpPacket>
   TmpPacket& Alloc() {
      assert(this->RBuf_.cfront() == nullptr);
      // +16 = for log header(uint32 PkSize; + enum TmpPktKind; + TimeStamp)
      char* pktmp = this->RBuf_.AllocPrefix(sizeof(TmpPacket) + 16) - sizeof(TmpPacket);
      this->RBuf_.SetPrefixUsed(pktmp);
      return *reinterpret_cast<TmpPacket*>(pktmp);
   }
};

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
/// 台灣期交所 的 「TCPIP/TMP 連線協定」處理程序.
/// - 結算會員委託/成交回報(ApCode=ReportCm:8), TMPDC Session(ApCode=TmpDc:7) 不能下單.
class f9twf_API ExgLineTmpSession : public fon9::io::Session {
   fon9_NON_COPY_NON_MOVE(ExgLineTmpSession);
   using base = fon9::io::Session;

   fon9::io::Device* Dev_{};
   fon9::TimeStamp   LastRxTime_;
   fon9::TimeStamp   LastTxTime_;
   ExgLineTmpLog     Log_;
   std::string       DevOpenArgs_;

   // 來自期交所的 L50.HeartBtInt;
   fon9::TimeInterval   ApHbInterval_;
   // 來自期交所的 L50.max_flow_ctrl_cnt;
   uint16_t             MaxFlowCtrlCnt_;
   // 來自期交所的 L30;
   ExgSystemType        SystemType_;

   enum class TmpSt {
      DevClose,
      TmpRelink,
      ApBroken,
      ApReady,
   };
   TmpSt TmpSt_{};

   void CheckApBroken(TmpSt st);
   void AsyncClose(std::string cause);
   void WriteLogLinkSt(const fon9::io::StateUpdatedArgs& e);
   void OnRecvTmpLinkSys(const TmpHeaderSt& pktmp);
   bool OnRecvTmpApPacket(const TmpHeader& pktmp);

protected:
   void OnDevice_Initialized(fon9::io::Device& dev) override;
   bool OnDevice_BeforeOpen(fon9::io::Device& dev, std::string& cfgstr) override;
   void OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) override;
   void OnDevice_StateUpdated(fon9::io::Device& dev, const fon9::io::StateUpdatedArgs& e) override;
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device& dev) override;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueueList& rxbuf) override;
   void OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) override;

   virtual void OnExgTmp_ApReady() = 0;
   /// 只有在 ApReady => 不是 Ready 時; 才會通知一次.
   virtual void OnExgTmp_ApBroken() = 0;
   virtual void OnExgTmp_ApPacket(const TmpHeader& pktmp) = 0;

public:
   ExgIoManager&        IoManager_;
   const ExgLineTmpArgs LineArgs_;

   /// log 必須已開啟成功, 否則會直接 crash!
   ExgLineTmpSession(ExgIoManager&         ioMgr,
                     const ExgLineTmpArgs& lineArgs,
                     ExgLineTmpLog&&       log)
      : Log_(std::move(log))
      , IoManager_(ioMgr)
      , LineArgs_(lineArgs) {
      assert(this->Log_.IsReady());
   }

   ~ExgLineTmpSession();

   /// 來自期交所的 L50.max_flow_ctrl_cnt;
   uint16_t MaxFlowCtrlCnt() const { return this->MaxFlowCtrlCnt_; }
   /// 來自期交所的 L30;
   ExgSystemType SystemType() const { return this->SystemType_; }

   /// - 您必須自行先填妥的欄位: TmpHeader::MsgLength_、MsgSeqNum_、MessageType_;
   /// - 傳送前自動填入的欄位: TmpHeader::MsgTime_、FcmId_、SessionId_、CheckSum;
   void SendTmpNoSeqNum(fon9::TimeStamp now, ExgLineTmpRevBuffer&& buf);

   /// - 您必須自行先填妥的欄位: TmpHeader::MsgLength_、MessageType_;
   /// - 傳送前自動填入的欄位: TmpHeader::MsgTime_、FcmId_、SessionId_、MsgSeqNum_、CheckSum;
   /// - MsgSeqNum_ 不做任何鎖定保護, 所以呼叫端必須自行確保不會重複進入.
   ///   - 線路管理員 fon9::fmkt::TradingLineManager 的 SendRequestImpl(); 已有鎖定保護.
   void SendTmpAddSeqNum(fon9::TimeStamp now, ExgLineTmpRevBuffer&& buf) {
      TmpHeader* pktmp = reinterpret_cast<TmpHeader*>(const_cast<char*>(buf.RBuf_.GetCurrent()));
      TmpPutValue(pktmp->MsgSeqNum_, this->Log_.FetchTxSeqNum());
      this->SendTmpNoSeqNum(now, std::move(buf));
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__f9twf_ExgLineTmpSession_hpp__
