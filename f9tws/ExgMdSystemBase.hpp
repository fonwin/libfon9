// \file f9tws/ExgMdSystemBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdSystemBase_hpp__
#define __f9tws_ExgMdSystemBase_hpp__
#include "f9tws/ExgMdFmt.hpp"
#include "fon9/PkCont.hpp"
#include "fon9/fmkt/MdSystem.hpp"

namespace f9tws {

class f9tws_API ExgMdSystemBase;

fon9_EXPORT_TEMPLATE_CLASS(f9tws_API, ExgMdHandler,       fon9::PkHandlerBase,   ExgMdSystemBase, ExgMdHead);
fon9_EXPORT_TEMPLATE_CLASS(f9tws_API, ExgMdHandlerAnySeq, fon9::PkHandlerAnySeq, ExgMdHandler);
using ExgMdHandlerSP = std::unique_ptr<ExgMdHandler>;

class f9tws_API ExgMdHandlerPkCont : public fon9::PkHandlerPkCont<ExgMdHandler> {
   fon9_NON_COPY_NON_MOVE(ExgMdHandlerPkCont);
   using base = fon9::PkHandlerPkCont<ExgMdHandler>;
public:
   using base::base;
   virtual ~ExgMdHandlerPkCont();

   void CheckLogLost(const void* pk, SeqT seq) {
      base::CheckLogLost(pk, seq, &OnLogSeqGap);
   }
   /// 在 rbuf 前端加上: "TwsExgMd.Gap|mkt=N|FmtNo=V"
   static void OnLogSeqGap(fon9::PkContFeeder& rthis, fon9::RevBuffer& rbuf, const void* pk);
};
//--------------------------------------------------------------------------//
/// - [上市、上櫃] 為不同的系統.
/// - 封包處理流程:
///   => ExgMdReceiverSession 收到封包(CheckSum OK): ExgMdReceiverSession::OnPkReceived()
///   => ExgMdSystemBase::OnPkReceived(): 根據 mkt,fmt,ver 尋找註冊的 ExgMdHandler;
///   => 衍生自 ExgMdHandlerAnySeq: void OnPkReceived(const PkType& pk, unsigned pksz) override;
///    或衍生自 ExgMdHandlerPkCont: void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) override;
///      - 由衍生者完成最後封包解析及後續處理。
///      - 使用者必須在系統建立時, 將封包處理者透過 RegMdMessageHandler<>() 註冊;
/// - 可能衍生出:
///   - ExgMdSystem:    提供給 Rts 發行系統之用;
///   - ExgMdSystemXXX: 特殊的專用系統;
class f9tws_API ExgMdSystemBase : public fon9::fmkt::MdSystemWithHb {
   fon9_NON_COPY_NON_MOVE(ExgMdSystemBase);
   using base = fon9::fmkt::MdSystemWithHb;
   using MdHandlers = ExgMdMessageDispatcher<ExgMdHandlerSP>;
   MdHandlers  MdHandlers_;

protected:
   void OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath) override;

   // 行情斷線判斷:
   // - 針對 [Market + (第一IP:股票 or 第二IP:權證 or 第五IP:盤中零股)] 處理斷線事件?
   // - 每個 [IP來源] 可能使用不同的 [實體線路], 所以最好能判斷不同[IP來源] 的斷線事件.
   // - 可能的判斷方法:
   //   - 針對不同 IP來源, 建立不同的 MdSys?:
   //      f9fmkt_TradingMarket(TwSEC,TwOTC) + f9fmkt_TradingSessionId(Normal,OddLot) + f9tws_SymbKind(TwsTypes.h);
   //   - 由 ExgMdReceiverSession::OnDevice_CommonTimer() 通知 this->MdHandlers_; 觸發行情斷線事件?
   // - PS:
   // 傳送通道 | Interface | 上市 Multicast group | 傳送格式  　　　| 上櫃 Multicast group
   // -----------------------------------------------------------------------------------
   //  第一IP　|   1       |   224.0.100.100　　　| 股票及格式一  　|   224.0.30.30
   // 　　  　 |   2       |   224.0.200.200　　　| Fmt: 1,6 ...　 |   224.0.130.130
   // -----------------------------------------------------------------------------------
   //  第二IP　|   1       |   224.2.100.100　　　| 權證及格式一  　|   224.2.30.30
   // 　　  　 |   2       |   224.2.200.200　　　| Fmt: 1,17,18 　|   224.2.130.130
   // -----------------------------------------------------------------------------------
   //  第三IP　|   1       |   224.4.100.100　　　| 股票5秒行情快照 |   224.4.30.30
   // 　　  　 |   2       |   224.4.200.200　　　| 　　　　  　　　|   224.4.130.130
   // -----------------------------------------------------------------------------------
   //  第四IP　|   1       |   224.6.100.100　　　| 其他格式  　　　|   224.6.30.30
   // 　　  　 |   2       |   224.6.200.200　　　| 　　　　  　　　|   224.6.130.130
   // -----------------------------------------------------------------------------------
   //  第五IP　|   1       |   224.8.100.100　　　| 盤中零股  　　　|   224.8.30.30
   // 　　  　 |   2       |   224.8.200.200　　　| Fmt: 22,23 　　|   224.8.130.130
   // fon9::TimeInterval OnMdSystem_HbTimer(fon9::TimeStamp now) override;

public:
   using base::base;
   ~ExgMdSystemBase();

   template <ExgMdMarket mkt, uint8_t fmt, uint8_t ver>
   void RegMdMessageHandler(ExgMdHandlerSP handler) {
      this->MdHandlers_.Reg<mkt, fmt, ver>(std::move(handler));
   }

   /// Session 收到封包後, 丟到這裡分派給 handler 處理.
   void OnPkReceived(const ExgMdHead& pk, unsigned pksz) {
      if (ExgMdHandler* handler = this->MdHandlers_.Get(pk).get())
         handler->OnPkReceived(pk, pksz);
   }
};
using ExgMdSystemBaseSP = fon9::intrusive_ptr<ExgMdSystemBase>;

} // namespaces
#endif//__f9tws_ExgMdSystemBase_hpp__
