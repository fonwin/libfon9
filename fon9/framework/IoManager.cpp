/// \file fon9/framework/IoManager.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"
#include "fon9/seed/SysEnv.hpp"

namespace fon9 {

bool IoManagerArgs::SetIoServiceCfg(seed::PluginsHolder& holder, StrView iosvCfg) {
   StrView inValue = SbrFetchInsideNoTrim(iosvCfg);
   if (!inValue.IsNull())
      iosvCfg = inValue;
   if (fon9::StrTrim(&iosvCfg).Get1st() != '/') {
      this->IoServiceCfgstr_ = iosvCfg.ToString();
      this->IoServiceSrc_.reset();
      return true;
   }
   iosvCfg.SetBegin(iosvCfg.begin() + 1);
   this->IoServiceSrc_ = holder.Root_->GetSapling<fon9::IoManager>(iosvCfg);
   if (this->IoServiceSrc_)
      return true;
   iosvCfg.SetBegin(iosvCfg.begin() - 1);
   holder.SetPluginsSt(fon9::LogLevel::Error, this->Name_, ".SetIoService"
                       "|err=Unknown IoManager: ", iosvCfg);
   return false;
}
bool IoManagerArgs::SetTagValue(seed::PluginsHolder& holder, StrView tag, StrView value) {
   if (tag == "Name")
      this->Name_ = value.ToString();
   else if (tag == "DeviceFactoryPark" || tag == "DevFp")
      this->DeviceFactoryPark_ = seed::FetchNamedPark<DeviceFactoryPark>(*holder.Root_, value);
   else if (tag == "SessionFactoryPark" || tag == "SesFp")
      this->SessionFactoryPark_ = seed::FetchNamedPark<SessionFactoryPark>(*holder.Root_, value);
   else if (tag == "IoService" || tag == "Svc")
      this->IoServiceSrc_ = holder.Root_->GetSapling<IoManager>(value);
   else if (tag == "IoServiceCfg" || tag == "SvcCfg")
      this->SetIoServiceCfg(holder, value);
   else if (tag == "Config" || tag == "Cfg") {
      std::string  cfgfn = SysEnv_GetConfigPath(*holder.Root_).ToString();
      value.AppendTo(cfgfn);
      this->CfgFileName_ = StrView_ToNormalizeStr(&cfgfn);
   }
   else
      return false;
   return true;
}
//--------------------------------------------------------------------------//
IoManager::IoManager(const IoManagerArgs& args)
   : Name_{args.Name_}
   , SessionFactoryPark_{args.SessionFactoryPark_ ? args.SessionFactoryPark_ : new SessionFactoryPark{"SessionFactoryPark"}}
   , DeviceFactoryPark_{args.DeviceFactoryPark_ ? args.DeviceFactoryPark_ : new DeviceFactoryPark{"DeviceFactoryPark"}}
   , IoServiceCfgstr_{args.IoServiceCfgstr_} {
   if (args.IoServiceSrc_) {
   #ifdef __fon9_io_win_IocpService_hpp__
      this->IocpService_ = args.IoServiceSrc_->GetIocpService();
   #endif
   #ifdef __fon9_io_FdrService_hpp__
      this->FdrService_ = args.IoServiceSrc_->GetFdrService();
   #endif
   }
}

void IoManager::ParseIoServiceArgs(io::IoServiceArgs& iosvArgs) const {
   RevBufferList rbuf{128};
   RevPutChar(rbuf, '\n');
   if (!ParseConfig(iosvArgs, &this->IoServiceCfgstr_, rbuf)) {
      // fon9_LOG_ERROR:
      RevPrint(rbuf, "IoManager.", this->Name_, ".CreateIoService|info=use default config");
      LogWrite(LogLevel::Error, std::move(rbuf));
   }
}

void IoManager::RaiseMakeIoServiceFatal(Result2 err) const {
   std::string errmsg = RevPrintTo<std::string>("IoManager.", this->Name_, ".MakeIoService|", err);
   fon9_LOG_FATAL(errmsg);
   Raise<std::runtime_error>(std::move(errmsg));
}

template <class IoService>
intrusive_ptr<IoService> IoManager::MakeIoService() const {
   io::IoServiceArgs iosvArgs;
   this->ParseIoServiceArgs(iosvArgs);
   typename IoService::MakeResult err;
   auto iosv = IoService::MakeService(iosvArgs, this->Name_, err);
   if (!iosv)
      this->RaiseMakeIoServiceFatal(err);
   return iosv;
}

#ifdef __fon9_io_win_IocpService_hpp__
io::IocpServiceSP IoManager::GetIocpService() {
   if (!this->IocpService_)
      this->IocpService_ = this->MakeIoService<io::IocpService>();
   return this->IocpService_;
}
#endif

#ifdef __fon9_io_FdrService_hpp__
io::FdrServiceSP IoManager::GetFdrService() {
   if (!this->FdrService_) {
      io::IoServiceArgs iosvArgs;
      this->ParseIoServiceArgs(iosvArgs);

      Result2  err;
      this->FdrService_ = MakeDefaultFdrService(iosvArgs, this->Name_, err);
      if (!this->FdrService_)
         this->RaiseMakeIoServiceFatal(err);
   }
   return this->FdrService_;
}
#endif

//--------------------------------------------------------------------------//
//
// 在 Device 還活著時, 不能把 DeviceMap 清除,
// 因為 dev 的事件通知 mgr 時, 還會透過 dev.GetManagerBookmark() 取回 item.
//
// /// 先呼叫 this->DisposeDevices(cause); 然後清除 DeviceMap;
// /// 在RAM的設定會消失, 但不會影響設定檔的內容.
// void IoManager::ClearSessions(std::string cause) {
//    DeviceMap::Locker map{this->DeviceMap_};
//    this->LockedDisposeDevices(map, std::move(cause));
//    map->clear();
// }

void IoManager::LockedDisposeDevices(const DeviceMap::Locker& map, std::string cause) {
   for (auto& i : *map) {
      i->SchSt_ = SchSt::Unknown;
      if (io::Device* dev = i->Device_.get())
         dev->AsyncDispose(cause);
   }
}
//--------------------------------------------------------------------------//
void IoManager::AssignStStr(CharVector& dst, TimeStamp now, StrView stmsg) {
   size_t sz = kDateTimeStrWidth + stmsg.size() + 1;
   // 底下 -+sizeof(NumOutBuf) 是因為 RevPrint() 需要分配較多的保留大小.
   // 但因我確定實際大小不會超過, 所以自行調整.
   RevBufferFixedMem rbuf{static_cast<char*>(dst.alloc(sz)) - sizeof(NumOutBuf), sz + sizeof(NumOutBuf)};
   RevPrint(rbuf, now, '|', stmsg);
}
bool IoManager::AddConfig(StrView id, const IoConfigItem& cfg) {
   DeviceItemSP      item{new DeviceItem{id, cfg}};
   DeviceMap::Locker curmap{this->DeviceMap_};
   if (curmap->insert(item).second)
      return this->CheckOpenDevice(*item) == DeviceOpenResult::NewDeviceCreated;
   return false;
}
IoManager::DeviceOpenResult IoManager::CreateDevice(DeviceItem& item) {
   if (item.Device_)
      return DeviceOpenResult::AlreadyExists;
   item.SessionSt_.clear();
   item.DeviceSt_.clear();
   auto sesFactory = this->SessionFactoryPark_->Get(ToStrView(item.Config_.SessionName_));
   if (!sesFactory) {
      AssignStStr(item.SessionSt_, UtcNow(), "Session factory not found.");
      return DeviceOpenResult::SessionFactoryNotFound;
   }
   if (auto devFactory = this->DeviceFactoryPark_->Get(ToStrView(item.Config_.DeviceName_))) {
      std::string errReason;
      if (!(item.Device_ = devFactory->CreateDevice(this, *sesFactory, item.Config_, errReason))) {
         errReason = "IoManager.CreateDevice|err=" + errReason;
         AssignStStr(item.DeviceSt_, UtcNow(), &errReason);
         return DeviceOpenResult::DeviceCreateError;
      }
      item.Device_->SetManagerBookmark(reinterpret_cast<io::Bookmark>(&item));
      item.Device_->Initialize();
      return DeviceOpenResult::NewDeviceCreated;
   }
   AssignStStr(item.DeviceSt_, UtcNow(), "Device factory not found");
   return DeviceOpenResult::DeviceFactoryNotFound;
}
IoManager::DeviceOpenResult IoManager::CheckCreateDevice(DeviceItem& item) {
   if (item.Config_.Enabled_ != EnabledYN::Yes)
      return DeviceOpenResult::Disabled;
   if (item.Device_)
      return DeviceOpenResult::AlreadyExists;
   DeviceOpenResult res = this->CreateDevice(item);
   if (!item.Device_) {
      fon9_LOG_WARN("IoManager.", this->Name_, ".CheckCreateDevice"
                    "|cfgid=", item.Id_,
                    "|ses=", item.Config_.SessionName_,
                    "|sesArgs={", item.Config_.SessionArgs_, "}"
                    "|dev=", item.Config_.DeviceName_,
                    "|devArgs={", item.Config_.DeviceArgs_, "}"
                    "|res=", res,
                    "|err=", item.SessionSt_, '|', item.DeviceSt_);
   }
   return res;
}
IoManager::DeviceOpenResult IoManager::CheckOpenDevice(DeviceItem& item) {
   DeviceOpenResult res = this->CheckCreateDevice(item);
   if(res == DeviceOpenResult::Disabled)
      return res;
   if (res == DeviceOpenResult::AlreadyExists) {
      if (item.CfgDevArgs_ == item.Config_.DeviceArgs_)
         return DeviceOpenResult::AlreadyExists;
      // 不用 return: 因為 Config 的 DeviceArgs_ 有變動, 需要重新開啟.
   }
   if (item.Device_) {
      AssignStStr(item.DeviceSt_, UtcNow(), "Async opening");
      item.CfgDevArgs_ = item.Config_.DeviceArgs_;
      item.Device_->AsyncOpen(item.Config_.DeviceArgs_.ToString());
   }
   return res;
}
//--------------------------------------------------------------------------//
IoManager::DeviceRun* IoManager::FromManagerBookmark(io::Device& dev) {
   DeviceRun* item = reinterpret_cast<DeviceRun*>(dev.GetManagerBookmark());
   if (item && item->Device_.get() == &dev)
      return item;
   return nullptr;
}
void IoManager::OnDevice_Initialized(io::Device&) {
   // accepted client 在 OnDevice_Accepted() 時設定 SetManagerBookmark();
   // 一般 device 在 Device::Initialize() 之後設定 SetManagerBookmark();
   // 所以這裡不用做任何事情.
}
void IoManager::OnDevice_Destructing(io::Device& dev) {
   if (DeviceRun* item = reinterpret_cast<DeviceRun*>(dev.GetManagerBookmark())) {
      // 只有 AcceptedClient 在 OnDevice_Destructing() 時還會保留 ManagerBookmark.
      // 所以此時的 item 可以安全的刪除.
      delete item;
      // TODO: 如果 dev 是 AcceptedClient, 則更新 server st (accepted client 的剩餘數量)?
   }
   fon9_LOG_TRACE("IoManager.", this->Name_, ".OnDevice_Destructing|dev=", ToPtr(&dev));
}
void IoManager::OnDevice_Accepted(io::DeviceServer& server, io::DeviceAcceptedClient& client) {
   DeviceRun* cliItem = new DeviceRun; // 在 OnDevice_Destructing() 時刪除.
   cliItem->Device_.reset(&client);
   cliItem->AcceptedClientSeq_ = client.GetAcceptedClientSeq();
   client.SetManagerBookmark(reinterpret_cast<io::Bookmark>(cliItem));

   DeviceMap::Locker map{this->DeviceMap_};
   if (auto serverItem = this->FromManagerBookmark(server)) {
      this->MakeAcceptedClientTree(*serverItem, client.Owner_);
      cliItem->Sapling_ = serverItem->Sapling_;
   }
}
void IoManager::OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) {
   this->UpdateDeviceState(dev, e.After_);
}
void IoManager::OnDevice_StateUpdated(io::Device& dev, const io::StateUpdatedArgs& e) {
   this->UpdateDeviceState(dev, e);
}
void IoManager::UpdateDeviceState(io::Device& dev, const io::StateUpdatedArgs& e) {
   if (io::DeviceAcceptedClient* cli = dynamic_cast<io::DeviceAcceptedClient*>(&dev)) {
      auto clis = cli->Owner_->Lock();
      this->UpdateDeviceStateLocked(dev, e);
   }
   else {
      DeviceMap::Locker map{this->DeviceMap_};
      this->UpdateDeviceStateLocked(dev, e);
   }
}
void IoManager::UpdateDeviceStateLocked(io::Device& dev, const io::StateUpdatedArgs& e) {
   RevBufferList rbuf{128};
   DeviceRun*    item = this->FromManagerBookmark(dev);
   RevPrint(rbuf, '\n');
   if (e.State_ == io::State::Opening && item && item->IsDeviceItem()) {
      RevPrint(rbuf, // "SessionName={SessionArgs}|DeviceName={DeviceArgs}
               '|',  static_cast<DeviceItem*>(item)->Config_.SessionName_,
               "={", static_cast<DeviceItem*>(item)->Config_.SessionArgs_, "}"
               "|",  static_cast<DeviceItem*>(item)->Config_.DeviceName_, 
               "={", static_cast<DeviceItem*>(item)->Config_.DeviceArgs_, '}');
   }
   else {
      RevPrint(rbuf, "|id={", e.DeviceId_, "}" "|info=", e.Info_);
   }
   RevPrint(rbuf, "|st=", GetStateStr(e.State_));
   if (item) {
      // dev 狀態改變時, ses 的狀態必定已經過期), 所以清除 SessionSt_;
      // 這樣 ses 就 **不用考慮** 斷線後重設 ses 狀態.
      item->SessionSt_.clear();
      const BufferNode* bnode = rbuf.cfront();
      char* pmsg = static_cast<char*>(item->DeviceSt_.alloc(kDateTimeStrWidth + CalcDataSize(bnode)));
      ToStrRev(pmsg += kDateTimeStrWidth, UtcNow());
      pmsg = static_cast<char*>(CopyNodeList(pmsg, bnode));
      *(pmsg - 1) = 0; // for back '\n' => '\0';
      item->DeviceSt_.resize(item->DeviceSt_.size() - 1); // remove back '\n'
      if (e.State_ == io::State::Opening)
         item->OpenArgs_.assign(e.Info_);
      else if (e.State_ == io::State::Disposing) {
         item->Device_.reset();
         item->Sapling_.reset();
         if (item->AcceptedClientSeq_ == 0)
            dev.SetManagerBookmark(0);
         // item->AcceptedClientSeq_ != 0, accepted client:
         // 保留 Bookmark, 等 OnDevice_Destructing() 事件時, 將 item 刪除.
      }
      this->NotifyChanged(*item);
      if (item->IsDeviceItem()) {
         if (e.State_ == io::State::Disposing)
            static_cast<DeviceItem*>(item)->SchSt_ = SchSt::Unknown;
         RevPrint(rbuf, "|cfgid=", static_cast<DeviceItem*>(item)->Id_);
      }
   }
   static const LogLevel lvs[] {
      LogLevel::Trace, // Initializing,
      LogLevel::Trace, // Initialized,
      LogLevel::Warn,  // ConfigError,
      LogLevel::Info,  // Opening,
      LogLevel::Info,  // WaitingLinkIn,
      LogLevel::Info,  // Linking,
      LogLevel::Warn,  // LinkError,
      LogLevel::Info,  // LinkReady,
      LogLevel::Warn,  // LinkBroken,
      LogLevel::Info,  // Listening,
      LogLevel::Warn,  // ListenBroken,
      LogLevel::Trace, // Lingering,
      LogLevel::Trace, // Closing,
      LogLevel::Info,  // Closed,
      LogLevel::Info,  // Disposing,
      LogLevel::Info,  // Destructing,
   };
   const LogLevel lv = static_cast<unsigned>(e.State_) < numofele(lvs)
                     ? lvs[static_cast<unsigned>(e.State_)] : LogLevel::Fatal;
   if (fon9_UNLIKELY(lv >= LogLevel_)) {
      RevPrint(rbuf, "IoManager.", this->Name_, ".DeviceState|dev=", ToPtr(&dev));
      LogWrite(lv, std::move(rbuf));
   }
}
void IoManager::OnSession_StateUpdated(io::Device& dev, StrView stmsg, LogLevel lv) {
   if (io::DeviceAcceptedClient* cli = dynamic_cast<io::DeviceAcceptedClient*>(&dev)) {
      auto clis = cli->Owner_->Lock();
      this->UpdateSessionStateLocked(dev, stmsg, lv);
   }
   else {
      DeviceMap::Locker map{this->DeviceMap_};
      this->UpdateSessionStateLocked(dev, stmsg, lv);
   }
}
void IoManager::UpdateSessionStateLocked(io::Device& dev, StrView stmsg, LogLevel lv) {
   auto item = this->FromManagerBookmark(dev);
   if(item) {
      AssignStStr(item->SessionSt_, UtcNow(), stmsg);
      this->NotifyChanged(*item);
   }
   if (fon9_UNLIKELY(lv >= fon9::LogLevel_)) {
      fon9::RevBufferList rbuf{fon9::kLogBlockNodeSize};
      fon9::RevPutChar(rbuf, '\n');
      fon9::RevPrint(rbuf, "|st=", stmsg);
      if (item && item->IsDeviceItem())
         RevPrint(rbuf, "|cfgid=", static_cast<DeviceItem*>(item)->Id_);
      fon9::RevPrint(rbuf, "IoManager.", this->Name_, ".SessionState|dev=", ToPtr(&dev));
      fon9::LogWrite(lv, std::move(rbuf));
   }
}
//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4355 /* 'this' : used in base member initializer list*/);
struct IoManager::AcceptedTree : public seed::Tree {
   fon9_NON_COPY_NON_MOVE(AcceptedTree);
   using base = seed::Tree;
   using Locker = io::AcceptedClients::ConstLocker;
   struct PodOp : public seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = PodOpDefault;
      io::DeviceSP Device_;
      Locker&      Locker_;
      PodOp(io::Device* seed, Tree& sender, const StrView& key, Locker& locker)
         : base{sender, seed::OpResult::no_error, key}
         , Device_{seed}
         , Locker_(locker) {
      }
      void Lock() {
         if (!this->Locker_.owns_lock())
            this->Locker_.lock();
      }
      void Unlock() {
         if (this->Locker_.owns_lock())
            this->Locker_.unlock();
      }
      void BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) override {
         this->Lock();
         if (auto item = IoManager::FromManagerBookmark(*this->Device_))
            this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawRd{*item});
         else
            base::BeginRead(tab, std::move(fnCallback));
      }
      void OnSeedCommand(seed::Tab* tab, StrView cmdln, seed::FnCommandResultHandler resHandler) override {
         this->Unlock();
         this->Tab_ = tab;
         std::string msg = this->Device_->DeviceCommand(cmdln);
         resHandler(*this, &msg);
      }
   };
   struct TreeOp : public seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = seed::TreeOp;
      TreeOp(Tree& tree) : base(tree) {
      }
      static void MakePolicyRecordView(io::AcceptedClientsImpl::const_iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
         if (tab) {
            if (auto item = IoManager::FromManagerBookmark(**ivalue))
               FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{*item}, rbuf, seed::GridViewResult::kCellSplitter);
         }
         RevPrint(rbuf, (*ivalue)->GetAcceptedClientSeq());
      }
      void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) override {
         seed::GridViewResult res{this->Tree_, req.Tab_};
         {
            auto  map = static_cast<AcceptedTree*>(&this->Tree_)->Listener_->Lock();
            seed::MakeGridView(*map, seed::GetIteratorForGv(*map, req.OrigKey_),
                               req, res, &MakePolicyRecordView);
         } // unlock.
         fnCallback(res);
      }
      void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
         {
            auto  map = static_cast<AcceptedTree*>(&this->Tree_)->Listener_->Lock();
            auto  ifind = seed::GetIteratorForPod(*map, strKeyText);
            if (ifind != map->end()) {
               PodOp op(*ifind, this->Tree_, strKeyText, map);
               fnCallback(op, &op);
               return;
            }
         } // unlock.
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
      }
   };
public:
   const io::DeviceListenerSP Listener_;
   AcceptedTree(io::DeviceListenerSP&& listener)
      : base{IoManager::GetAcceptedClientLayout()}
      , Listener_{std::move(listener)} {
   }
   void OnTreeOp(seed::FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
   }
};
fon9_WARN_POP;

seed::LayoutSP IoManager::GetAcceptedClientLayout() {
   static seed::LayoutSP layout{MakeAcceptedClientLayoutImpl()};
   return layout;
}
seed::LayoutSP IoManager::MakeAcceptedClientLayoutImpl() {
   seed::Fields fields;
   fields.Add(fon9_MakeField2(DeviceRun, SessionSt));
   fields.Add(fon9_MakeField2(DeviceRun, DeviceSt));
   fields.Add(fon9_MakeField2(DeviceRun, OpenArgs));
   seed::TabSP tabSt{new seed::Tab(Named{"Status"}, std::move(fields), seed::TabFlag::HasSameCommandsSet)};
   return new seed::Layout1(seed::MakeField(Named{"Id"}, 0, *static_cast<const io::DeviceAcceptedClientSeq*>(nullptr)),
                            std::move(tabSt));
}
void IoManager::MakeAcceptedClientTree(DeviceRun& serverItem, io::DeviceListenerSP listener) {
   if (!serverItem.Sapling_ || static_cast<AcceptedTree*>(serverItem.Sapling_.get())->Listener_ != listener)
      serverItem.Sapling_.reset(new AcceptedTree{std::move(listener)});
}
//--------------------------------------------------------------------------//
bool IoManager::DeviceItem::AsyncDisposeDevice(TimeStamp now, StrView cause) {
   this->SchSt_ = SchSt::Unknown;
   if (this->Device_) {
      // 應該在 if (e.State_ == io::State::Disposing) 時清除 ManagerBookmark,
      // 否則會造成 this leak.
      // this->Device_->SetManagerBookmark(0);
      this->Device_->AsyncDispose(cause.ToString());
      return true;
   }
   this->SessionSt_.clear();
   AssignStStr(this->DeviceSt_, now, cause);
   return false;
}

} // namespaces
