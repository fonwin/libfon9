// \file f9tws/ExgMktPlayer.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMktPkReceiver.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/buffer/FwdBufferList.hpp"
#include "fon9/framework/IoFactory.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/File.hpp"

namespace f9tws {

struct ExgMktPlayerArgs {
   fon9::File::PosType  RdFrom_{0};
   fon9::File::PosType  RdTo_{0};
   uint64_t             RdBytes_{0};
   fon9::TimeInterval   RdInterval_{fon9::TimeInterval_Millisecond(1)};
   fon9::TimeInterval   AtEnd_;
};
class ExgMktPlayer : public ExgMktPkReceiver, public fon9::io::Session {
   fon9_NON_COPY_NON_MOVE(ExgMktPlayer);
   fon9::io::Device*    Device_{nullptr};
   fon9::DcQueueList    RdBuffer_;
   fon9::File           MktFile_;
   ExgMktPlayerArgs     Args_;
   fon9::TimeStamp      LastStTime_;
   uint64_t             SentPkCount_{0};

   struct NodeSend : public fon9::BufferNodeVirtual {
      fon9_NON_COPY_NON_MOVE(NodeSend);
      using base = fon9::BufferNodeVirtual;
      friend class fon9::BufferNode;// for BufferNode::Alloc();
      ExgMktPlayer* Player_{nullptr};
      using base::base;
   protected:
      void OnBufferConsumed() override {
         if (auto player = this->Player_)
            player->Device_->CommonTimerRunAfter(player->Args_.RdInterval_);
      }
      void OnBufferConsumedErr(const fon9::ErrC&) override {}
   public:
      static void Send(ExgMktPlayer& player, const void* pk, unsigned pksz) {
         fon9::BufferList buf;
         fon9::AppendToBuffer(buf, pk, pksz);
         buf.push_back(base::Alloc<NodeSend>(0, StyleFlag{}));
         player.Device_->SendBuffered(std::move(buf));
      }
      static void Wait(ExgMktPlayer& player) {
         fon9::BufferList buf;
         NodeSend*        node;
         buf.push_back(node = base::Alloc<NodeSend>(0, StyleFlag{}));
         node->Player_ = &player;
         player.Device_->SendBuffered(std::move(buf));
      }
   };

   void OnDevice_Initialized(fon9::io::Device& dev) override {
      this->Device_ = &dev;
   }
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device&) {
      this->ReadMktFile(fon9::UtcNow());
      return fon9::io::RecvBufferSize::NoRecvEvent;
   }
   void OnDevice_CommonTimer(fon9::io::Device&, fon9::TimeStamp now) override {
      this->ReadMktFile(now);
   }

   template <class... ArgsT>
   std::string RdInfoStr(ArgsT&&... args) {
      return fon9::RevPrintTo<std::string>("Pk=", this->SentPkCount_, "|Pos=", this->Args_.RdFrom_, std::forward<ArgsT>(args)...);
   }

   void ReadMktFile(fon9::TimeStamp now) {
      if (this->Device_->OpImpl_GetState() != fon9::io::State::LinkReady)
         return;
      if (this->Args_.RdTo_ != 0 && this->Args_.RdTo_ <= this->Args_.RdFrom_) {
         this->Device_->Manager_->OnSession_StateUpdated(*this->Device_,
                                                         fon9::ToStrView(this->RdInfoStr("|End=", this->Args_.RdTo_)),
                                                         fon9::LogLevel::Info);
         return;
      }
      auto node = fon9::FwdBufferNode::Alloc(this->Args_.RdBytes_ ? this->Args_.RdBytes_ : 1024);
      auto rdsz = this->Args_.RdBytes_ ? this->Args_.RdBytes_ : node->GetRemainSize();
      if (this->Args_.RdTo_ != 0) {
         auto remain = this->Args_.RdTo_ - this->Args_.RdFrom_;
         if (rdsz > remain)
            rdsz = remain;
      }
      auto rdres = this->MktFile_.Read(this->Args_.RdFrom_, node->GetDataEnd(), rdsz);
      std::string    stmsg;
      fon9::LogLevel stlv = fon9::LogLevel::Trace;
      if (rdres.HasResult()) {
         if (rdres.GetResult() > 0) {
            this->Args_.RdFrom_ += rdres.GetResult();
            node->SetDataEnd(node->GetDataEnd() + rdres.GetResult());
            this->RdBuffer_.push_back(node);
            node = nullptr;
            this->FeedBuffer(this->RdBuffer_);
            if (now - this->LastStTime_ >= fon9::TimeInterval_Second(1)) {
               this->LastStTime_ = now;
               stmsg = this->RdInfoStr();
            }
            NodeSend::Wait(*this);
         }
         else if (this->Args_.AtEnd_.GetOrigValue() > 0) {
            stmsg = this->RdInfoStr("|WaitNew");
            this->Device_->CommonTimerRunAfter(this->Args_.AtEnd_);
         }
         else {
            stmsg = this->RdInfoStr("|AtEnd");
            stlv = fon9::LogLevel::Info;
         }
      }
      else { // read error.
         stmsg = this->RdInfoStr("|Rd=", rdres);
         stlv = fon9::LogLevel::Info;
      }
      if (node)
         fon9::FreeNode(node);
      if (!stmsg.empty())
         this->Device_->Manager_->OnSession_StateUpdated(*this->Device_, &stmsg, stlv);
   }

   bool OnPkReceived(const void* pk, unsigned pksz) override {
      ++this->SentPkCount_;
      NodeSend::Send(*this, pk, pksz);
      return true;
   }
public:
   ExgMktPlayer(fon9::File&& fd, const ExgMktPlayerArgs& args)
      : MktFile_{std::move(fd)}
      , Args_(args) {
   }
   ~ExgMktPlayer() {
   }
};

class ExgMktPlayerFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMktPlayerFactory);
   using base = fon9::SessionFactory;
public:
   using base::base;

   static fon9::io::SessionSP CreateSession(fon9::StrView cfg, std::string& errReason) {
      // cfg.SessionArgs_: fileName|From=pos|To=pos|Rd=BlockSize|Interval=ti|
      // TODO: std::string ExgMktPlayer::SessionCommand(Device& dev, StrView cmdln);
      //    - pause
      //    - restart [from to speed]
      fon9::StrView     fn = fon9::StrFetchTrim(cfg, '|');
      ExgMktPlayerArgs  args;
      fon9::StrView     tag, value;
      while (fon9::StrFetchTagValue(cfg, tag, value)) {
         if (tag == "From")
            args.RdFrom_ = fon9::StrTo(value, args.RdFrom_);
         else if (tag == "To")
            args.RdTo_ = fon9::StrTo(value, args.RdTo_);
         else if (tag == "Rd")
            args.RdBytes_ = fon9::StrTo(value, args.RdBytes_);
         else if (tag == "Interval")
            args.RdInterval_ = fon9::StrTo(value, args.RdInterval_);
         else if (tag == "AtEnd")
            args.AtEnd_ = fon9::StrTo(value, args.AtEnd_);
         else {
            errReason = fon9::RevPrintTo<std::string>("Create:TwsExgMktPlayer|err=Unknown tag: ", tag);
            return fon9::io::SessionSP{};
         }
      }
      fon9::File fd;
      auto       res = fd.Open(fn.ToString(), fon9::FileMode::Read);
      if (!res) {
         errReason = fon9::RevPrintTo<std::string>("Create:TwsExgMktPlayer|fname=", fd.GetOpenName(), '|', res);
         return fon9::io::SessionSP{};
      }
      return fon9::io::SessionSP{new ExgMktPlayer{std::move(fd), args}};
   }
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
      (void)mgr;
      return this->CreateSession(fon9::ToStrView(cfg.SessionArgs_), errReason);
   }
   struct Server : public fon9::io::SessionServer {
      fon9_NON_COPY_NON_MOVE(Server);
      std::string Config_;
      Server(std::string cfg) : Config_{std::move(cfg)} {
      }
      fon9::io::SessionSP OnDevice_Accepted(fon9::io::DeviceServer&) override {
         std::string errReason;
         return ExgMktPlayerFactory::CreateSession(&this->Config_, errReason);
      }
   };
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
      (void)mgr; (void)errReason;
      return fon9::io::SessionServerSP{new Server(cfg.SessionArgs_.ToString())};
   }
};

static bool TwsExgMktPlayer_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   (void)args; // plugins args: "Name=ExgMktPlayer|AddTo=FpSession"
   struct ArgsParser : public fon9::SessionFactoryConfigParser {
      using base = fon9::SessionFactoryConfigParser;
      ArgsParser() : base{"TwsExgMktPlayer"} {
      }
      fon9::SessionFactorySP CreateSessionFactory() override {
         return fon9::SessionFactorySP{new ExgMktPlayerFactory{this->Name_}};
      }
   };
   return ArgsParser{}.Parse(holder, args);
}

} // namespaces

extern "C" f9tws_API fon9::seed::PluginsDesc f9p_TwsExgMktPlayer;
fon9::seed::PluginsDesc f9p_TwsExgMktPlayer{"", &f9tws::TwsExgMktPlayer_Start, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"TwsExgMktPlayer", &f9p_TwsExgMktPlayer};
