// \file f9twf/ExgMiSystemBase.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMiSystemBase_hpp__
#define __f9twf_ExgMiSystemBase_hpp__
#include "f9twf/ExgMiReceiver.hpp"
#include "fon9/fmkt/MdSystem.hpp"
#include "fon9/framework/IoManager.hpp"

namespace f9twf {

/// - [日盤、夜盤] 為不同的系統.
///   - 每個盤別, 各自有相同的 Channel, 例: Channel.1=期貨行情, Channel.2=選擇權行情;
/// - 封包處理流程:
///   => ExgMiReceiver(io:Session) 收到封包(CheckSum OK): ExgMiReceiver::OnPkReceived()
///   => ExgMiChannel::OnPkReceived(): 根據 tx,mg,ver 尋找註冊的 ExgMiHandler;
///   => 衍生自 ExgMiHandlerAnySeq: void OnPkReceived(const PkType& pk, unsigned pksz) override;
///    或衍生自 ExgMiHandlerPkCont: void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) override;
///      - 由衍生者完成最後封包解析及後續處理。
///      - 使用者必須在系統建立時, 將封包處理者註冊到 Channel::RegMiHandler<>();
/// - 可能衍生出:
///   - ExgMiSystemRts: 提供給 Rts 發行系統之用;
///   - ExgMiSystemXXX: 特殊的專用系統;
class f9twf_API ExgMiSystemBase : public fon9::fmkt::MdSystem {
   fon9_NON_COPY_NON_MOVE(ExgMiSystemBase);
   using base = fon9::fmkt::MdSystem;

   std::array<ExgMiChannelSP, 2> Channels_;
   void Ctor();

   static void EmitHbTimer(fon9::TimerEntry* timer, fon9::TimeStamp now);
   fon9::DataMemberEmitOnTimer<&ExgMiSystemBase::EmitHbTimer> HbTimer_;

protected:
   void OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath) override;

public:
   const f9fmkt_TradingSessionId TradingSessionId_;
   char                          Padding____[7];

   using NoDataEvent = std::function<void(ExgMiChannel&)>;
   using NoDataEventSubject = fon9::Subject<NoDataEvent>;
   NoDataEventSubject   NoDataEventSubject_;

   template <class... ArgsT>
   ExgMiSystemBase(f9fmkt_TradingSessionId txSessionId, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , TradingSessionId_{txSessionId} {
      assert(txSessionId == f9fmkt_TradingSessionId_Normal || txSessionId == f9fmkt_TradingSessionId_AfterHour);
      this->Ctor();
   }

   virtual ~ExgMiSystemBase();

   /// - 建立一個 IoMgr 在 this->Sapling_;
   /// - root 是為了取得 :
   ///   - fon9::seed::FetchNamedPark<fon9::DeviceFactoryPark>(root, "FpDevice");
   ///   - fon9::seed::SysEnv_GetConfigPath(root)
   /// - iomArgs 只會用到
   ///   - IoManagerSP IoServiceSrc_;
   ///   - std::string IoServiceCfgstr_;
   /// - 返回時 iomArgs.IoServiceSrc_ 會填入新加入的 IoManager;
   void PlantIoMgr(fon9::seed::MaTree& root, fon9::IoManagerArgs& iomArgs);

   ExgMiChannel* GetChannel(uint8_t channelId) {
      if (--channelId < this->Channels_.size())
         return this->Channels_[channelId].get();
      return nullptr;
   }
};
using ExgMiSystemBaseSP = fon9::intrusive_ptr<ExgMiSystemBase>;

} // namespaces
#endif//__f9twf_ExgMiSystemBase_hpp__
