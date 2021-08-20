/// \file fon9/rc/RcSv_UT.cpp
///
/// 測試項目: RcSv CAPI: RcClientApi.h
///
/// \author fonwinz@gmail.com
#include "fon9/TestTools.hpp"
#include "fon9/io/SimpleManager.hpp"
#include "fon9/rc/RcClientSession.hpp"

#include "fon9/framework/Framework.hpp"
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/auth/SaslScramSha256Server.hpp"
#include "fon9/auth/PolicyAcl.hpp"

#define kUSERID      "user"
#define kPASSWORD    "pencil"
static const char kRcSv_UT_Dbf_FileName[] = "RcSv_UT.f9dbf";
static const char kRcSv_Log_FileName[] = "RcSv_UT.log";
//--------------------------------------------------------------------------//
#ifdef _DEBUG
// 通知 Server 端暫停執行訂閱, 等收到[取消訂閱]後, 才執行上次暫停的[訂閱要求].
extern fon9_API bool gIs_RcSv_UT_Server_ReorderRunOnce;
#endif

bool gIsRcFuncHeartbeatPauseReq;
bool gIsRcFuncHeartbeatPausing;

class RcFuncHeartbeatPauser : public fon9::rc::RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFuncHeartbeatPauser);
   using base = RcFunctionAgent;
public:
   RcFuncHeartbeatPauser() : base{f9rc_FunctionCode_Heartbeat} {
   }
   void OnRecvFunctionCall(fon9::rc::RcSession& ses, fon9::rc::RcFunctionParam& param) override {
      fon9::RevBufferList  rbuf{64};
      char*                pout = rbuf.AllocPrefix(param.RemainParamSize_);
      auto                 rdsz = param.RecvBuffer_.Read(pout -= param.RemainParamSize_, param.RemainParamSize_);
      (void)rdsz; assert(rdsz == param.RemainParamSize_);
      rbuf.SetPrefixUsed(pout);
      ToBitv(rbuf, param.RecvTime_);
      ses.Send(f9rc_FunctionCode_Heartbeat, std::move(rbuf));
      if (gIsRcFuncHeartbeatPauseReq) {
         puts("\n[RcSvr:Paused]");
         gIsRcFuncHeartbeatPausing = true;
         while (gIsRcFuncHeartbeatPauseReq)
            std::this_thread::yield();
         puts("\n[RcSvr:Resumed]");
         gIsRcFuncHeartbeatPausing = false;
      }
   }
};
void PauseServer(fon9::rc::RcClientSession* ses) {
   puts("[RcSvr:Pausing..]");
   gIsRcFuncHeartbeatPauseReq = true;
   fon9::RevBufferList rbuf{128};
   fon9::ToBitv(rbuf, fon9::UtcNow());
   ses->Send(f9rc_FunctionCode_Heartbeat, std::move(rbuf));
   while (!gIsRcFuncHeartbeatPausing)
      std::this_thread::yield();
}
void ResumeServer() {
   puts("[RcSvr:Resuming..]");
   gIsRcFuncHeartbeatPauseReq = false;
   while (gIsRcFuncHeartbeatPausing)
      std::this_thread::yield();
}
//--------------------------------------------------------------------------//
extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpClient;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSessionServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSvServerAgent;
void* ForceLinkSomething() {
   static const void* forceLinkList[]{
      &f9p_NamedIoManager, &f9p_TcpServer, &f9p_TcpClient,
      &f9p_RcSessionServer, &f9p_RcSvServerAgent,
   };
   return forceLinkList;
}

struct RcServerFramework : private fon9::Framework {
   RcServerFramework() {
      ForceLinkSomething();
      this->Initialize();
   }
   ~RcServerFramework() {
      this->DisposeForAppQuit();
      this->RemoveTestFiles();
   }
   void RemoveTestFiles() {
      ::remove(kRcSv_UT_Dbf_FileName);
   }
private:
   // 建立一個萬事不支援的 Tree, 用來測試.
   struct DummyTree : public fon9::seed::Tree {
      fon9_NON_COPY_NON_MOVE(DummyTree);
      using base = fon9::seed::Tree;
      using base::base;
      struct TreeOp : public fon9::seed::TreeOp {
         fon9_NON_COPY_NON_MOVE(TreeOp);
         using base = fon9::seed::TreeOp;
         using base::base;
      };
      void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override {
         TreeOp op{*this};
         fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
      }
   };
   void Initialize() {
      this->Root_.reset(new fon9::seed::MaTree{"RcSv_UT"});
      this->Root_->AddNamedSapling(new DummyTree{this->Root_->LayoutSP_}, "DummyTree");
      // 開檔前先移除上次的測試檔, 避免遺留的資料影響測試結果.
      this->RemoveTestFiles();
      // AuthMgr 必須有一個「有效的 InnDbf」儲存資料, 否則無法運作.
      fon9::InnDbfSP authStorage{new fon9::InnDbf("userdbf", nullptr)};
      authStorage->Open(kRcSv_UT_Dbf_FileName);
      // f9rc server 需要 AuthMgr 提供使用者驗證.
      this->MaAuth_ = fon9::auth::AuthMgr::Plant(this->Root_, authStorage, "MaAuth");
      // 測試 SeedVisitor 必須有 PoAcl;
      auto  poAcl = fon9::auth::PolicyAclAgent::Plant(*this->MaAuth_);
      poAcl->GetSapling()->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opTree) {
         opTree->Add(kUSERID, [](const fon9::seed::PodOpResult& res, fon9::seed::PodOp* opPod) {
            auto tab = res.Sender_->LayoutSP_->GetTab(0);
            opPod->BeginWrite(*tab, [tab](const fon9::seed::SeedOpResult&, const fon9::seed::RawWr* wr) {
               tab->Fields_.Get("HomePath")->StrToCell(*wr, "/");
               tab->Fields_.Get("MaxSubrCount")->StrToCell(*wr, "10");
               tab->Fields_.Get("FcQryCount")->StrToCell(*wr, "3");
            });
            opPod->GetSapling(*tab)->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opAclTree) {
               opAclTree->Add("/", [](const fon9::seed::PodOpResult& resAc, fon9::seed::PodOp* opAcPod) {
                  auto tabAc = resAc.Sender_->LayoutSP_->GetTab(0);
                  opAcPod->BeginWrite(*tabAc, [tabAc](const fon9::seed::SeedOpResult&, const fon9::seed::RawWr* wr) {
                     tabAc->Fields_.Get("Rights")->StrToCell(*wr, "xff");
                  });
               });
            });
         });
      });
      // 建立一個可登入的 kUSERID + kPASSWORD
      auto  userMgr = fon9::auth::PlantScramSha256(*this->MaAuth_);
      userMgr->GetSapling()->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* opTree) {
         opTree->Add(kUSERID, [](const fon9::seed::PodOpResult& res, fon9::seed::PodOp* opPod) {
            auto tab = res.Sender_->LayoutSP_->GetTab(0);
            opPod->BeginWrite(*tab, [tab](const fon9::seed::SeedOpResult&, const fon9::seed::RawWr* wr) {
               tab->Fields_.Get("RoleId")->StrToCell(*wr, kUSERID);
               tab->Fields_.Get("Flags")->PutNumber(*wr, 0, 0);
            });
            opPod->OnSeedCommand(tab, "repw " kPASSWORD, [](const fon9::seed::SeedOpResult&, fon9::StrView) {});
         });
      });
      this->MaPlugins_.reset(new fon9::seed::PluginsMgr(this->Root_, "MaPlugins"));
      this->Root_->Add(this->MaPlugins_, "Plant.Plugins");
      #define _ "\x01"
      this->MaPlugins_->LoadConfigStr(
         "Id"      _ "Enabled" _ "FileName" _ "EntryName"       _ "Args\n"
         "DevTcpc" _ "Y"       _ ""         _ "TcpClient"       _ "Name=TcpClient|AddTo=FpDevice\n"
         "DevTcps" _ "Y"       _ ""         _ "TcpServer"       _ "Name=TcpServer|AddTo=FpDevice\n"
         "MaIo"    _ "Y"       _ ""         _ "NamedIoManager"  _ "Name=MaIo|DevFp=FpDevice|SesFp=FpSession|SvcCfg='ThreadCount=1|Capacity=100'\n"
         "MaIo2"   _ "Y"       _ ""         _ "NamedIoManager"  _ "\n"
         "RcSvr"   _ "Y"       _ ""         _ "RcSessionServer" _ "Name=RcSvr|AuthMgr=MaAuth|AddTo=FpSession\n"
         "RcSvs"   _ "Y"       _ ""         _ "RcSvServerAgent" _ "AddTo=FpSession/RcSvr\n"
      );
      auto iomgr = this->Root_->GetSapling<fon9::IoManagerTree>("MaIo");
      auto sesfac = fon9::dynamic_pointer_cast<fon9::rc::RcFunctionMgr>(iomgr->SessionFactoryPark_->Get("RcSvr"));
      sesfac->Reset(fon9::rc::RcFunctionAgentSP{new RcFuncHeartbeatPauser});
      #define kSERVER_AFTER_OPEN_MS  10
      iomgr->LoadConfigStr(
         "Id"    _ "Enabled" _ "Sch" _ "Session" _ "SessionArgs" _ "Device"    _ "DeviceArgs\n"
         "iRcSv" _ "Y"       _ ""    _ "RcSvr"   _ ""            _ "TcpServer" _ "19999\n"
         "iTest" _ "Y"       _ ""    _ "TestSes" _ ""            _ ""          _ "\n"
         , fon9::TimeInterval_Millisecond(kSERVER_AFTER_OPEN_MS)
      );
   }
};
//--------------------------------------------------------------------------//
#include "fon9/rc/RcSeedVisitorClient.hpp"
#include "fon9/rc/RcSeedVisitor.h"
#include "fon9/ConsoleIO.h"
#include "fon9/CtrlBreakHandler.h"

typedef struct {
   f9rc_ClientSession*        Session_;
   /// SvConfig_->RightsTables_ 只能在 OnSvConfig() 事件裡面安全的使用,
   /// 其他地方使用 SvConfig_->RightsTables_ 都不安全.
   const f9sv_ClientConfig*   SvConfig_;

   fon9::rc::RcClientSession*         RcCliSes_;
   fon9::rc::RcSeedVisitorClientNote* RcCliNote_;
} UserDefine;
//--------------------------------------------------------------------------//
void PrintEvSplit(const char* evName) {
   printf(">>>>>>>>>>> %s: ", evName);
}
void PrintEvEnd() {
   puts("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
}
void PrintSeedValues(const f9sv_ClientReport* rpt) {
   if (rpt->Seed_ == NULL)
      return;
   printf("[%s/%s]", rpt->Tab_->Named_.Name_.Begin_, rpt->SeedKey_.Begin_);
   const f9sv_Field* fld = rpt->Tab_->FieldArray_;
   unsigned fldidx = rpt->Tab_->FieldCount_;
   if (fldidx <= 0)
      return;
   for (;;) {
      char  buf[1024];
      printf("|%s=[%s]", fld->Named_.Name_.Begin_,
             f9sv_GetField_StrN(rpt->Seed_, fld, buf, sizeof(buf)));
      if (--fldidx <= 0)
         break;
      ++fld;
   }
   puts("");
}

void fon9_CAPI_CALL OnClientLinkEv(f9rc_ClientSession* ses, f9io_State st, fon9_CStrView info) {
   (void)ses; (void)st; (void)info;
   PrintEvSplit("OnClientLinkEv");
   printf("%d:%s\n", st, info.Begin_);
   PrintEvEnd();
}
void fon9_CAPI_CALL OnSvConfig(f9rc_ClientSession* ses, const f9sv_ClientConfig* cfg) {
   PrintEvSplit("OnSvConfig");
   printf("FcQry=%u/%u|MaxSubrCount=%u\n" "{%s}\n",
          cfg->FcQryCount_, cfg->FcQryMS_, cfg->MaxSubrCount_,
          cfg->RightsTables_.OrigStrView_.Begin_);
   static_cast<UserDefine*>(ses->UserData_)->SvConfig_ = cfg;
   PrintEvEnd();
}

void CreateSessionConnection(UserDefine& ud) {
   f9rc_ClientSessionParams  f9rcCliParams;
   memset(&f9rcCliParams, 0, sizeof(f9rcCliParams));
   f9rcCliParams.DevName_    = "TcpClient";
   f9rcCliParams.LogFlags_   = f9rc_ClientLogFlag_All;
   f9rcCliParams.FnOnLinkEv_ = &OnClientLinkEv;
   f9rcCliParams.UserId_     = kUSERID;
   f9rcCliParams.Password_   = kPASSWORD;
   f9rcCliParams.DevParams_  = "127.0.0.1:19999";
   // ----------------------------
   f9sv_ClientSessionParams   svRcParams;
   f9sv_InitClientSessionParams(&f9rcCliParams, &svRcParams);
   svRcParams.FnOnConfig_ = &OnSvConfig;

   memset(&ud, 0, sizeof(ud));
   f9rcCliParams.UserData_ = &ud;
   f9rc_CreateClientSession(&ud.Session_, &f9rcCliParams);
   ud.RcCliSes_ = static_cast<fon9::rc::RcClientSession*>(ud.Session_);
   ud.RcCliNote_ = static_cast<fon9::rc::RcSeedVisitorClientNote*>(ud.RcCliSes_->GetNote(f9rc_FunctionCode_SeedVisitor));

   puts("Waiting for connection...");
   while (ud.SvConfig_ == NULL) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   puts(ud.SvConfig_ ? "Connection ready." : "Wait connection timeout.");
}
//--------------------------------------------------------------------------//
const f9sv_Field* FindFieldName(const f9sv_Tab* tab, fon9::StrView fldName) {
   uint32_t L = tab->FieldCount_;
   if (L > 0) {
      const f9sv_Field* fld = tab->FieldArray_ + L;
      do {
         --fld;
         if (fldName == fld->Named_.Name_)
            return fld;
      } while (--L > 0);
   }
   return NULL;
}
//--------------------------------------------------------------------------//
struct ChkItem {
   f9sv_Result    ResultCode_;
   char           Padding___[4];
   fon9::StrView  TabName_;
   fon9::StrView  KeyText_;
   fon9::StrView  Fields_;
   fon9::StrView  ExResult_;
   ChkItem(f9sv_Result rc, fon9::StrView exResult = nullptr)
      : ResultCode_{rc}
      , ExResult_{exResult} {
   }
   ChkItem(f9sv_Result rc, fon9::StrView tabName, fon9::StrView keyText, fon9::StrView fields, fon9::StrView exResult = nullptr)
      : ResultCode_{rc}
      , TabName_{tabName}
      , KeyText_{keyText}
      , Fields_{fields}
      , ExResult_{exResult} {
   }
   ChkItem(fon9::StrView tabName, fon9::StrView keyText, fon9::StrView fields, fon9::StrView exResult = nullptr)
      : ChkItem(f9sv_Result_NoError, tabName, keyText, fields, exResult) {
   }
   template <class... ArgsT>
   ChkItem(fon9::seed::OpResult rc, ArgsT&&... args)
      : ChkItem(static_cast<f9sv_Result>(rc), std::forward<ArgsT>(args)...) {
   }
};
using ChkItemList = std::vector<ChkItem>;
//--------------------------------------------------------------------------//
struct ReqChecker {
   fon9::StrView  ItemName_;
   f9sv_SeedName  Seed_;
   ChkItemList    ChkItemList_;
   unsigned       ChkNextIndex_{0};
   char           Padding____[4];

   template <class... ArgsT>
   ReqChecker(fon9::StrView itemName, const f9sv_SeedName& seed, ArgsT&&... args)
      : ItemName_{itemName}
      , Seed_(seed)
      , ChkItemList_{std::forward<ArgsT>(args)...} {
   }
   ReqChecker() = default;
   ReqChecker(ReqChecker&& rhs) = default;
   ReqChecker& operator=(ReqChecker&& rhs) = default;
   ReqChecker(ReqChecker& rhs) = default;
   ReqChecker& operator=(ReqChecker& rhs) = default;

   virtual ~ReqChecker() {
      bool hasNoRecv = false;
      for (unsigned L = this->ChkNextIndex_; L < this->ChkItemList_.size(); ++L) {
         const ChkItem& item = this->ChkItemList_[L];
         this->PrintItemName(L);
         printf("NoRecv:%s\n", item.Fields_.begin());
         hasNoRecv = true;
      }
      if (hasNoRecv)
         abort();
   }
   void RewindCheck() {
      this->ChkNextIndex_ = 0;
   }
   void Wait(unsigned tailSize = 0) const {
      while (this->ChkNextIndex_ < this->ChkItemList_.size() - tailSize)
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   void PrintItemName(unsigned idx) const {
      std::string msg = fon9::RevPrintTo<std::string>(
         "[TEST ] TestItem=", this->ItemName_, '[', fon9::ToPtr(this), "].[", idx, "][");
      if (idx < this->ChkItemList_.size()) {
         const ChkItem& item = this->ChkItemList_[idx];
         if (item.ResultCode_ == f9sv_Result_NoError)
            fon9::RevPrintAppendTo(msg, item.TabName_, '/', item.KeyText_);
         else {
            fon9::RevPrintAppendTo(msg, item.ResultCode_, ':', f9sv_GetSvResultMessage(item.ResultCode_));
         }
      }
      fon9::RevPrintAppendTo(msg, "] ");
      printf("%s", msg.c_str());
   }
   void PrintResultCodeNotMatch(const ChkItem& item, const f9sv_ClientReport* rpt) {
      (void)item;
      printf("Result not match|rpt=%d:%s" "\r[ERROR]\n",
             rpt->ResultCode_, f9sv_GetSvResultMessage(rpt->ResultCode_));
   }

   virtual void CheckResult(const f9sv_ClientReport* rpt) {
      this->PrintItemName(this->ChkNextIndex_);
      if (this->ChkNextIndex_ >= this->ChkItemList_.size()) {
         printf("Unexpected(too more data?)|rpt=%d:%s" "\r[ERROR]\n",
                rpt->ResultCode_, f9sv_GetSvResultMessage(rpt->ResultCode_));
         PrintSeedValues(rpt);
         PrintEvEnd();
         abort();
         return;
      }
      const ChkItem& item = this->ChkItemList_[this->ChkNextIndex_];
      if (!item.ExResult_.IsNull()) {
         if (item.ExResult_ != rpt->ExResult_) {
            printf("ExResult={%s}|rpt={%s}" "\r[ERROR]\n",
                   item.ExResult_.begin(), rpt->ExResult_.Begin_);
            abort();
            return;
         }
      }
      if (item.ResultCode_ != rpt->ResultCode_) {
         this->PrintResultCodeNotMatch(item, rpt);
         abort();
         return;
      }
      if (!item.TabName_.IsNull() && item.TabName_ != rpt->Tab_->Named_.Name_) {
         printf("TabName not matched|rpt=%s" "\r[ERROR]", rpt->Tab_->Named_.Name_.Begin_);
         abort();
         return;
      }
      if (!item.KeyText_.IsNull() && item.KeyText_ != rpt->SeedKey_) {
         printf("KeyText not matched|rpt=%s" "\r[ERROR]\n", rpt->SeedKey_.Begin_);
         abort();
         return;
      }
      fon9::StrView flds = item.Fields_;
      if (flds.empty()) {
         puts("\r[OK   ]");
         ++this->ChkNextIndex_;
         return;
      }
      bool  isError = false;
      do {
         fon9::StrView val = fon9::StrFetchNoTrim(flds, '=');
         if (const f9sv_Field* fld = FindFieldName(rpt->Tab_, val)) {
            val = fon9::StrFetchNoTrim(flds, '\x1');
            char  buf[1024];
            if (val != fon9::StrView_cstr(f9sv_GetField_StrN(rpt->Seed_, fld, buf, sizeof(buf)))) {
               printf("Field[%s] value[%s] not match|got=%s",
                      fld->Named_.Name_.Begin_, val.ToString().c_str(), buf);
               isError = true;
            }
         }
         else {
            printf("Field[%s] not found.", val.ToString().c_str());
            isError = true;
         }
      } while (!flds.empty());
      if (isError)
         abort();
      else {
         printf("Matchs: %s" "\r[OK   ]\n", item.Fields_.begin());
         ++this->ChkNextIndex_;
      }
   }

   static void OnReport(f9rc_ClientSession*, const f9sv_ClientReport* rpt) {
      static_cast<ReqChecker*>(rpt->UserData_)->CheckResult(rpt);
   }

   template <typename Fn, class... ArgsT>
   f9sv_Result Run(f9rc_ClientSession* ses, Fn fn, ArgsT&&... args) {
      f9sv_ReportHandler handler;
      handler.UserData_   = this;
      handler.FnOnReport_ = &ReqChecker::OnReport;
      return fn(ses, &this->Seed_, handler, std::forward<ArgsT>(args)...);
   }
};
struct ReqCheckerGV : public ReqChecker {
   fon9_NON_COPY_NON_MOVE(ReqCheckerGV);
   ReqCheckerGV() = delete;

   using base = ReqChecker;
   using base::base;
   void CheckResult(const f9sv_ClientReport* rpt) override {
      const ChkItem& item = this->ChkItemList_[this->ChkNextIndex_];
      if (item.ResultCode_ != rpt->ResultCode_) {
         this->PrintItemName(this->ChkNextIndex_++);
         this->PrintResultCodeNotMatch(item, rpt);
         abort();
      }
      else if (item.ResultCode_ != f9sv_Result_NoError) {
         this->PrintItemName(this->ChkNextIndex_++);
         puts("\r[OK   ]");
      }
      else {
         f9sv_ClientReport nrpt = *rpt;
         for (;;) {
            nrpt.SeedKey_ = f9sv_GridView_Parser(rpt, &nrpt.ExResult_);
            if (nrpt.SeedKey_.Begin_ == nullptr)
               break;
            base::CheckResult(&nrpt);
         }
      }
   }
};
//--------------------------------------------------------------------------//
#define MAKE_TEST_ITEM_STR(req, cmprc, fn, ...)                                     \
         fon9::RevPrintTo<std::string>(req.ItemName_, '[', fon9::ToPtr(&req), "] "  \
                                       #cmprc " (", res, ":" #fn ")")
// ----------------------------
#define RUN_TEST_REQ(req, cmprc, ...)                             \
         res = req.Run(ud.RcCliSes_, __VA_ARGS__);                \
         fon9::CheckTestResult(                                   \
            MAKE_TEST_ITEM_STR(req, cmprc, __VA_ARGS__).c_str(),  \
            __FILE__ ":" fon9_CTXTOCSTR(__LINE__),                \
            cmprc(res) )
//-////////////////////////////////////////////////////////////////////////-//
// 測試訂閱在 PendingReqs 的行為.
// test case: (1)訂閱放到 PendingReqs[0]; (2)取消訂閱.
// test case: (1)訂閱放到 PendingReqs[0]; (2)增加一筆查詢, (3)取消訂閱.
struct TestPending {
   UserDefine  ud;
   TestPending() {
      CreateSessionConnection(this->ud);
      PauseServer(this->ud.RcCliSes_);
      this->Err_UnsubNoSub(f9sv_SeedName{"xxx", "\t", "", 0, ""});
   }
   ~TestPending() {
      this->DestroySession(nullptr);
   }
   void DestroySession(ReqChecker* req) {
      if (this->ud.Session_) {
         ResumeServer();
         if (req)
            req->Wait();
         f9rc_DestroyClientSession_Wait(this->ud.Session_);
         this->ud.Session_ = nullptr;
      }
   }
   // 取消一個不存在的訂閱.
   void Err_UnsubNoSub(const f9sv_SeedName& seedName) {
      f9sv_Result  res;
      ReqChecker   unsuR("Err_UnsubNoSub", seedName, ChkItem(fon9::seed::OpResult::no_subscribe));
      RUN_TEST_REQ(unsuR, static_cast<f9sv_Result>(fon9::seed::OpResult::no_subscribe) == , f9sv_Unsubscribe);
   }
   // 重複訂閱測試.
   void Err_ReSubr(const f9sv_SeedName& seedName) {
      f9sv_Result  res;
      ReqChecker   subrR("Err_ReSubr", seedName, ChkItem(f9sv_Result_InSubscribe));
      RUN_TEST_REQ(subrR, f9sv_Result_InSubscribe == , f9sv_Subscribe);
   }
   // 一般訂閱成功的測試, 不用考慮是否有 Pending.
   // 執行訂閱前, 測試: 取消一個不存在的訂閱: Err_UnsubNoSub();
   // 執行訂閱後, 測試: 重複訂閱: Err_ReSubr();
   void RunSubrExChk(ReqChecker& subr0) {
      this->Err_UnsubNoSub(subr0.Seed_);
      f9sv_Result  res;
      RUN_TEST_REQ(subr0, f9sv_Result_NoError == , f9sv_Subscribe);
      this->Err_ReSubr(subr0.Seed_);
   }
   void RunSubrOnly(ReqChecker& subr0, const f9sv_Result exres) {
      f9sv_Result  res;
      RUN_TEST_REQ(subr0, exres == , f9sv_Subscribe);
   }
   // PendingReqs: [0]=subr0, [1]=unsu1:不論在之前是否有其他指令(例:qry), 取消訂閱必定會放在 subr 的下一個位置.
   void RunUnsub(ReqChecker& unsu1) {
      f9sv_Result res;
      RUN_TEST_REQ(unsu1, f9sv_Result_NoError == , f9sv_Unsubscribe);
      // 重複訂閱/重複取消, 都應回覆: f9sv_Result_InUnsubscribe
      ReqChecker   unsu1x("Err_ReX", unsu1.Seed_, ChkItem(f9sv_Result_InUnsubscribe));
      RUN_TEST_REQ(unsu1x, f9sv_Result_InUnsubscribe == , f9sv_Subscribe);
      unsu1x.RewindCheck();
      RUN_TEST_REQ(unsu1x, f9sv_Result_InUnsubscribe == , f9sv_Unsubscribe);
   }
   // 取消「不是」在 PendingReqs[0] 的訂閱:
   // 立即取消, 並移除訂閱要求, Internal.Unsubscribed, 不會送出
   // => [訂閱/取消] 流程結束.
   void RunUnsuInternal(ReqChecker& unsu) {
      f9sv_Result res;
      RUN_TEST_REQ(unsu, f9sv_Result_Unsubscribed == , f9sv_Unsubscribe);
      // 已取消, 再次取消.
      this->Err_UnsubNoSub(unsu.Seed_);
   }
   void RunQry(ReqChecker& qry1) {
      f9sv_Result res;
      RUN_TEST_REQ(qry1, f9sv_Result_NoError == , f9sv_Query);
      // 重複查詢.
      ReqChecker   qry1x("Err_ReQry", qry1.Seed_, ChkItem(f9sv_Result_InQuery));
      RUN_TEST_REQ(qry1x, f9sv_Result_InQuery == , f9sv_Query);
   }
};
//--------------------------------------------------------------------------//
// 預期會 [訂閱成功] 的測試:
// - RcSvcReq::Run():   還沒 Layout 時的行為.
// - RcSvcAckSubscribe: 訂閱成功後的行為.
// - RcSvcReq::Run():   已有 Layout 的行為.
void TestPendingSubrMaIoTree(const char* testName, ReqChecker* qry) {
   printf("===== Start TestPendingSubrMaIoTree: <%s> =====\n", testName);
   TestPending test1;
   // ----------------------------------------------
   // 沒 Layout 之前的測試: 先將要求放到 PendingReqs;
   ReqChecker  subr0("subr0", f9sv_SeedName{"/MaIo", "\t", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   ReqChecker  unsu1("unsu1", subr0.Seed_,
                     ChkItem(f9sv_Result_NoError, "Status", "iRcSv", "Session=RcSvr" _ "SessionSt=" _ "Device=TcpServer" _ "OpenArgs=19999"),
                     ChkItem(f9sv_Result_NoError, "Status", "iTest", "Session=TestSes"),
                     ChkItem(f9sv_Result_Unsubscribed));

   test1.RunSubrExChk(subr0);
   if (qry)
      test1.RunQry(*qry);
   test1.RunUnsub(unsu1);

   ReqChecker  subr2("subr2", f9sv_SeedName{"/MaIo", "iRcSv", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   ReqChecker  unsu2("unsu2", subr2.Seed_, ChkItem(f9sv_Result_Unsubscribed));
   test1.RunSubrExChk(subr2);
   test1.RunUnsuInternal(unsu2);

   ReqChecker  subr3("subr3", subr2.Seed_, ChkItem(fon9::seed::OpResult::not_supported_subscribe));
   test1.RunSubrExChk(subr3);

   ResumeServer();
   (qry ? qry : &subr3)->Wait();
   // ----------------------------------------------
   // "/MaIo" 已有 Layout, 訂閱 tree 成功後, 取消訂閱.
   ReqChecker subrOK("subrOK", f9sv_SeedName{"/MaIo", "\t", "", 0, ""},
      ChkItem(f9sv_Result_NoError), // 會先有一個訂閱成功, 且 rpt->Seed_ == NULL 的通知.
      ChkItem(f9sv_Result_NoError, "Status", "iRcSv", "Session=RcSvr" _ "SessionSt=" _ "Device=TcpServer" _ "OpenArgs=19999"),
      ChkItem(f9sv_Result_NoError, "Status", "iTest", "Session=TestSes"),
      ChkItem(f9sv_Result_Unsubscribing));
   test1.RunSubrExChk(subrOK);
   subrOK.Wait(1);
   // --- 確定訂閱成功後, 取消訂閱.
   PauseServer(test1.ud.RcCliSes_);
   ReqChecker unsuOK("unsuOK", subrOK.Seed_, ChkItem(f9sv_Result_Unsubscribed));
   test1.RunUnsub(unsuOK);
   // --- 測試: 有 Layout 時的查詢.
   if (qry) {
      qry->RewindCheck();
      test1.RunQry(*qry);
   }
   ResumeServer();
   (qry ? qry : &unsuOK)->Wait();
   // ----------------------------------------------
   // 訂閱/取消: 在 Server 端還在處理訂閱路徑時, 就收到了取消訂閱.
   // Server 端有可能正在走訂閱流程, 還沒走完就收到取消, 此時就不會回覆 Seeds.
#ifdef _DEBUG
   PauseServer(test1.ud.RcCliSes_);
   gIs_RcSv_UT_Server_ReorderRunOnce = true;
   ReqChecker subrRo("subrRo", f9sv_SeedName{"/MaIo", "\t", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   test1.RunSubrExChk(subrRo);
   ReqChecker unsuRo("unsuRo", subrRo.Seed_, ChkItem(f9sv_Result_Unsubscribed));
   test1.RunUnsub(unsuRo);
   ResumeServer();
   unsuRo.Wait();
#endif
   // ----------------------------------------------
   // 訂閱送出後, 尚未取得結果前, 立即取消.
   PauseServer(test1.ud.RcCliSes_);
   ReqChecker subrIm("subrIm", f9sv_SeedName{"/MaIo", "\t", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   test1.RunSubrExChk(subrIm);
   // 取消 subrIm: 此時會立即送出, 但因為已送出訂閱, 所以 Server 仍會回覆 Seeds 結果.
   ReqChecker unsuIm("unsuIm", subrIm.Seed_,
                    ChkItem(f9sv_Result_NoError, "Status", "iRcSv", "Session=RcSvr" _ "SessionSt=" _ "Device=TcpServer" _ "OpenArgs=19999"),
                    ChkItem(f9sv_Result_NoError, "Status", "iTest", "Session=TestSes"),
                    ChkItem(f9sv_Result_Unsubscribed));
   test1.RunUnsub(unsuIm);
   // ----------------------------------------------
   test1.DestroySession(&unsuIm);
   printf("##### End TestPendingSubrMaIoTree: <%s> #####\n", testName);
}
//--------------------------------------------------------------------------//
// 預期會 [有 Layout, 但訂閱失敗] 的測試:
// - RcSvcReq::Run():   還沒 Layout 時的行為.
// - RcSvcAckSubscribe: 訂閱失敗後的行為.
// - RcSvcReq::Run():   已有 Layout 的行為.
void TestPendingSubrMaIoKey(const char* testName, ReqChecker* qry) {
   printf("===== Start TestPendingSubrMaIoKey: <%s> =====\n", testName);
   TestPending test1;
   ReqChecker  subr0("subr0", f9sv_SeedName{"/MaIo", "KeyNotFound", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   ReqChecker  unsu1("unsu1", subr0.Seed_, ChkItem(f9sv_Result_NotFoundTreeOrKey));

   test1.RunSubrExChk(subr0);
   if (qry)
      test1.RunQry(*qry);
   test1.RunUnsub(unsu1);

   // 同一個 tree, 不同 key 的訂閱.
   ReqChecker  subr3("subr3", f9sv_SeedName{"/MaIo", "iRcSv", "", 0, ""}, ChkItem(fon9::seed::OpResult::not_supported_subscribe));
   test1.RunSubrExChk(subr3);

   ResumeServer();
   (qry ? qry : &subr3)->Wait();
   // ----------------------------------------------
   // 訂閱/取消: 在 Server 端還在處理訂閱路徑時, 就收到了取消訂閱.
   // Server 端有可能正在走訂閱流程, 還沒走完就收到取消, 此時就不會回覆 Seeds.
   // 即使是不支援訂閱的 seed, 也會回覆 f9sv_Result_Unsubscribed;
#ifdef _DEBUG
   PauseServer(test1.ud.RcCliSes_);
   gIs_RcSv_UT_Server_ReorderRunOnce = true;
   ReqChecker subrRo("subrRo", f9sv_SeedName{"/MaIo", "iRcSv", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   test1.RunSubrExChk(subrRo);
   ReqChecker unsuRo("unsuRo", subrRo.Seed_, ChkItem(f9sv_Result_Unsubscribed));
   test1.RunUnsub(unsuRo);
   ResumeServer();
   unsuRo.Wait();
#endif
   // ----------------------------------------------
   PauseServer(test1.ud.RcCliSes_);
   // 已有 Layout: 立即送出, 等 server 回覆
   // ----- 訂閱無效的 key;
   ReqChecker subr10("subr10", f9sv_SeedName{"/MaIo", "subr10", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
   test1.RunSubrExChk(subr10);
   // ----- 訂閱無效的 key, 並立即取消.
   ReqChecker subr11("subr11", f9sv_SeedName{"/MaIo", "subr11", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   test1.RunSubrExChk(subr11);
   ReqChecker unsu11("unsu11", f9sv_SeedName{"/MaIo", "subr11", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
   test1.RunUnsub(unsu11);
   // ----- 訂閱有效的 key, 但 seed 不支援訂閱
   ReqChecker subr20("subr20", f9sv_SeedName{"/MaIo", "iRcSv", "", 0, ""}, ChkItem(fon9::seed::OpResult::not_supported_subscribe));
   test1.RunSubrExChk(subr20);
   // ----- 訂閱有效的 key, 但 seed 不支援訂閱, 並立即取消.
   ReqChecker subr21("subr21", f9sv_SeedName{"/MaIo", "iTest", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   test1.RunSubrExChk(subr21);
   ReqChecker unsu21("unsu21", f9sv_SeedName{"/MaIo", "iTest", "", 0, ""}, ChkItem(fon9::seed::OpResult::not_supported_subscribe));
   test1.RunUnsub(unsu21);
   // ----------------------------------------------
   test1.DestroySession(&unsu21);
   printf("##### End TestPendingSubrMaIoKey: <%s> #####\n", testName);
}
//--------------------------------------------------------------------------//
// 預期會 [有 Layout, 但 TabNotFound] 的測試.
void TestPendingSubrMaIoNoTab(const char* testName, ReqChecker* qry) {
   printf("===== Start TestPendingSubrMaIoNoTab: <%s> =====\n", testName);
   TestPending test1;
   ReqChecker  subr0("subr0", f9sv_SeedName{"/MaIo", "\t", "BadTab", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
   ReqChecker  unsu1("unsu1", subr0.Seed_, ChkItem(f9sv_Result_NotFoundTab));

   test1.RunSubrExChk(subr0);
   if (qry)
      test1.RunQry(*qry);
   test1.RunUnsub(unsu1);

   ReqChecker  subr3("subr3", f9sv_SeedName{"/MaIo", "iRcSv", "", 0, ""}, ChkItem(fon9::seed::OpResult::not_supported_subscribe));
   test1.RunSubrExChk(subr3);

   ResumeServer();
   subr3.Wait();
   // ----------------------------------------------
   // 立即回覆 tab not found. 不會送出要求.
   ReqChecker  subrIm("subrIm", subr0.Seed_, ChkItem(f9sv_Result_NotFoundTab));
   test1.RunSubrOnly(subrIm, f9sv_Result_NotFoundTab);
   // ----------------------------------------------
   test1.DestroySession(nullptr);
   printf("##### End TestPendingSubrMaIoNoTab: <%s> #####\n", testName);
}
//-////////////////////////////////////////////////////////////////////////-//
// test case: 流量管制.
void WaitFlow(UserDefine& ud) {
   const auto fcti = ud.RcCliNote_->FcQryCheck(ud.RcCliNote_->LockTree(), fon9::UtcNow());
   if (fcti.GetOrigValue() <= 0)
      return;
   if (fcti < fon9::TimeInterval_Millisecond(100)) {
      std::this_thread::sleep_for(fcti.ToDuration());
      return;
   }
   f9sv_Result  res;
   ReqChecker   qryfc("qryfc:FlowControl", f9sv_SeedName{"/fc1", "", "", 0, ""}, ChkItem(f9sv_Result_FlowControl));
   RUN_TEST_REQ(qryfc, f9sv_Result_NoError <, f9sv_Query);
   std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<unsigned>(res)));
}
//-////////////////////////////////////////////////////////////////////////-//
int main(int argc, const char** argv) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   #endif
   fon9_SetConsoleUTF8();
   fon9_SetupCtrlBreakHandler();

   fon9::AutoPrintTestInfo utinfo("RcSv_UT");

   ::remove(kRcSv_Log_FileName);
   f9sv_Initialize(NULL); // log to console.
   //f9sv_Initialize(kRcSv_Log_FileName);
   {
      RcServerFramework rcServerFramework;
      // 等候建立 server.
      std::this_thread::sleep_for(std::chrono::milliseconds(kSERVER_AFTER_OPEN_MS + 100));
      // ----------------------------------------------
      ReqChecker qryMaIo("qryMaIo",
         f9sv_SeedName{"/MaIo", "iRcSv", "*", 0, ""},
         ChkItem("Status", "iRcSv", "Session=RcSvr" _ "SessionSt=" _ "Device=TcpServer" _ "OpenArgs=19999"),
         ChkItem("Config", "iRcSv", "Enabled=Y" _ "Sch=" _ "Session=RcSvr" _ "SessionArgs=" _ "Device=TcpServer" _ "DeviceArgs=19999")
      );
      TestPendingSubrMaIoTree("Subr+Qry", &qryMaIo);
      TestPendingSubrMaIoTree("Subr", nullptr);
      qryMaIo.RewindCheck();

      TestPendingSubrMaIoKey("Subr+Qry", &qryMaIo);
      TestPendingSubrMaIoKey("Subr", nullptr);
      qryMaIo.RewindCheck();

      TestPendingSubrMaIoNoTab("Subr+Qry", &qryMaIo);
      TestPendingSubrMaIoNoTab("Subr", nullptr);
      qryMaIo.RewindCheck();
      //-//////////////////////////////////////////////////////////////////-//
      UserDefine  ud;
      f9sv_Result res;
      CreateSessionConnection(ud);
      //==================================================================
      // 暫停 Server 之後, 測試 PendingReqs.
      // 暫停期間不可呼叫 ReqChecker::Wait();
      PauseServer(ud.RcCliSes_);
      //==================================================================
      // =============== /qry3: PendingReqs: [0]=qry3
      // test case: 查詢一個不存在的 tree; 此時應放到 PendingReqs, 然後再送一次查詢(此時應被拒絕:InQuery);
      ReqChecker   qry3("qry3:BadPath", f9sv_SeedName{"/qry3", "", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
      ReqChecker   qry4("qry4:InQuery", f9sv_SeedName{"/qry3", "", "", 0, ""}, ChkItem(f9sv_Result_InQuery));
      RUN_TEST_REQ(qry3, f9sv_Result_NoError == , f9sv_Query);
      RUN_TEST_REQ(qry4, f9sv_Result_InQuery == , f9sv_Query);
      // =============== /qry3: PendingReqs: [0]=qry3, [1]=subr5
      // test case: /qry3 還在 PendingReqs, 所以 subr5 會放在 PendingReqs 沒送出.
      ReqChecker   subr5("subr5", f9sv_SeedName{"/qry3", "", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribing));
      RUN_TEST_REQ(subr5, f9sv_Result_NoError == , f9sv_Subscribe);
      // 在 subr5 還存在於 PendingReqs 時, 再次重複訂閱: InSubscribe;
      ReqChecker   subr5x("subr5x", f9sv_SeedName{"/qry3", "", "", 0, ""}, ChkItem(f9sv_Result_InSubscribe));
      RUN_TEST_REQ(subr5x, f9sv_Result_InSubscribe == , f9sv_Subscribe);
      // =============== /qry3: PendingReqs: [0]=qry3
      // 在 subr5 尚未送出前就用 unsu5 取消訂閱: Internal.Unsubscribed;
      ReqChecker   unsu5("unsu5", f9sv_SeedName{"/qry3", "", "", 0, ""}, ChkItem(f9sv_Result_Unsubscribed));
      RUN_TEST_REQ(unsu5, f9sv_Result_Unsubscribed == , f9sv_Unsubscribe);
      // 已取消, 再次取消.
      ReqChecker   unsu5x("unsu5x", f9sv_SeedName{"/qry3", "", "", 0, ""}, ChkItem(fon9::seed::OpResult::no_subscribe));
      RUN_TEST_REQ(unsu5x, static_cast<f9sv_Result>(fon9::seed::OpResult::no_subscribe) == , f9sv_Unsubscribe);
      // =============== /qry3: PendingReqs: [0]=qry3, [1]=subr6
      // test case: /qry3 已取消訂閱後, 再次訂閱, 放入 PendingReqs, 等候結果.
      ReqChecker   subr6("subr6", f9sv_SeedName{"/qry3", "", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
      RUN_TEST_REQ(subr6, f9sv_Result_NoError == , f9sv_Subscribe);
      // =============== /qry3: PendingReqs: [0]=qry3, [1]=subr6, [2]=qry3a(key="a")
      ReqChecker   qry3a("qry3a", f9sv_SeedName{"/qry3", "a", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
      RUN_TEST_REQ(qry3a, f9sv_Result_NoError == , f9sv_Query);
      //==================================================================
      WaitFlow(ud);
      qryMaIo.RewindCheck();
      RUN_TEST_REQ(qryMaIo, f9sv_Result_NoError == , f9sv_Query);

      WaitFlow(ud);
      ReqCheckerGV gv1("gv1", f9sv_SeedName{"/MaPlugins", "", "*", 0, ""}, ChkItem(f9sv_Result_NotFoundTab));
      RUN_TEST_REQ(gv1, f9sv_Result_NoError == , f9sv_GridView, static_cast<int16_t>(10));

      WaitFlow(ud);
      ReqCheckerGV gv2("gv2",
         f9sv_SeedName{"/MaPlugins", "", "", 0, ""},
         ChkItem("Plugins", "DevTcpc", "Enabled=Y" _ "EntryName=TcpClient"       _ "Args=Name=TcpClient|AddTo=FpDevice"),
         ChkItem("Plugins", "DevTcps", "Enabled=Y" _ "EntryName=TcpServer"       _ "Args=Name=TcpServer|AddTo=FpDevice"),
         ChkItem("Plugins", "MaIo",    "Enabled=Y" _ "EntryName=NamedIoManager"  _ "Args=Name=MaIo|DevFp=FpDevice|SesFp=FpSession|SvcCfg='ThreadCount=1|Capacity=100'"),
         ChkItem("Plugins", "MaIo2",   "Enabled=Y" _ "EntryName=NamedIoManager"  _ "Args="),
         ChkItem("Plugins", "RcSvr",   "Enabled=Y" _ "EntryName=RcSessionServer" _ "Args=Name=RcSvr|AuthMgr=MaAuth|AddTo=FpSession"),
         ChkItem("Plugins", "RcSvs",   "Enabled=Y" _ "EntryName=RcSvServerAgent" _ "Args=AddTo=FpSession/RcSvr")
      );
      RUN_TEST_REQ(gv2, f9sv_Result_NoError == , f9sv_GridView, static_cast<int16_t>(10));

      // =============== f9sv_Write.
      WaitFlow(ud);
      ReqChecker wr1("wr1", f9sv_SeedName{"/MaAuth/PoAcl", "wr1key", "", 0, ""},
         ChkItem("Acl", "wr1key", "HomePath=/wr1Home" _ "MaxSubrCount=5" _ "FcQryCount=7", "")
      );
      RUN_TEST_REQ(wr1, f9sv_Result_NoError == , f9sv_Write, "HomePath=/wr1Home|MaxSubrCount=5|FcQryCount=7");
      // 正在處理 f9sv_Write("/MaAuth/PoAcl", "wr1key"); 必須等候結果, 才能對同一個 Key 進行操作.
      ReqChecker   rmInWr("rmInWr", wr1.Seed_, ChkItem(f9sv_Result_InWrite));
      RUN_TEST_REQ(rmInWr, f9sv_Result_InWrite == , f9sv_Remove);
      WaitFlow(ud);
      ReqChecker   wrBadTree("wrBadTree", f9sv_SeedName{"/xxx", "wrBadTree", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
      RUN_TEST_REQ(wrBadTree, f9sv_Result_NoError == , f9sv_Write, "");
      // ===============
      WaitFlow(ud);
      ReqCheckerGV gvDummy("gvDummy", f9sv_SeedName{"/DummyTree", "", "", 0, ""}, ChkItem(fon9::seed::OpResult::not_supported_grid_view));
      RUN_TEST_REQ(gvDummy, f9sv_Result_NoError == , f9sv_GridView, static_cast<int16_t>(10));
      //-//////////////////////////////////////////////////////////////////-//
      ResumeServer();
      gvDummy.Wait();
      // ----------------------------
      // 接下來測試: 因為 Layout 都已查到, 所以直接送出, 沒有放到 PendingReqs.
      WaitFlow(ud);
      // =============== f9sv_Query
      ReqChecker   qryNoKey("qryNoKey", f9sv_SeedName{"/MaAuth/PoAcl", "NoKey", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
      RUN_TEST_REQ(qryNoKey, f9sv_Result_NoError == , f9sv_Query);
      // =============== f9sv_Write: DummyTree 不支援 get pod;
      WaitFlow(ud);
      ReqChecker wrDummy("wrDummy", f9sv_SeedName{gvDummy.Seed_.TreePath_, "wrDummy", "", 0, ""},
         ChkItem(fon9::seed::OpResult::not_supported_get_pod)
      );
      RUN_TEST_REQ(wrDummy, f9sv_Result_NoError == , f9sv_Write, "");
      // ----- f9sv_Write 部份成功, 部分欄位錯誤.
      WaitFlow(ud);
      ReqChecker wr2("wr2", f9sv_SeedName{"/MaAuth/PoAcl", "wr2key", "", 0, ""},
         ChkItem("Acl", "wr2key", "HomePath=/wr2Home", "fieldName=xMaxSubrCount|err=field not found")
      );
      RUN_TEST_REQ(wr2, f9sv_Result_NoError == , f9sv_Write, "HomePath=/wr2Home|xMaxSubrCount=5");
      // =============== f9sv_Command.
      WaitFlow(ud);
      ReqChecker   cmd1("cmd1", f9sv_SeedName{"/MaAuth/UserSha256", kUSERID, "", 0, ""},
         ChkItem("User", kUSERID, "", "repw" _ "Reset password" _ "[NewPass] or Random new password."));
      RUN_TEST_REQ(cmd1, f9sv_Result_NoError == , f9sv_Command, "");
      WaitFlow(ud);
      ReqChecker   cmdNoKey("cmdNoKey", f9sv_SeedName{"/MaAuth/UserSha256", "NoKey", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
      RUN_TEST_REQ(cmdNoKey, f9sv_Result_NoError == , f9sv_Command, "");
      // =============== f9sv_Remove.
      WaitFlow(ud);
      ReqChecker   rm1("rm1", f9sv_SeedName{"/MaAuth/PoAcl", "wr1key", "", 0, ""}, ChkItem(f9sv_Result_RemovePod));
      RUN_TEST_REQ(rm1, f9sv_Result_NoError == , f9sv_Remove);
      WaitFlow(ud);
      ReqChecker   rmNoKey("rmNoKey", f9sv_SeedName{"/MaAuth/PoAcl", "NoKey", "", 0, ""}, ChkItem(f9sv_Result_NotFoundTreeOrKey));
      RUN_TEST_REQ(rmNoKey, f9sv_Result_NoError == , f9sv_Remove);
      // =============== gv 指定 start key;
      WaitFlow(ud);
      ReqCheckerGV gv3("gv3", f9sv_SeedName{"/MaPlugins", "RcSv", "", 0, ""},
         ChkItem("Plugins", "RcSvr", "Enabled=Y" _ "EntryName=RcSessionServer" _ "Args=Name=RcSvr|AuthMgr=MaAuth|AddTo=FpSession"),
         ChkItem("Plugins", "RcSvs", "Enabled=Y" _ "EntryName=RcSvServerAgent" _ "Args=AddTo=FpSession/RcSvr")
      );
      RUN_TEST_REQ(gv3, f9sv_Result_NoError == , f9sv_GridView, static_cast<int16_t>(10));
      // ============================
      WaitFlow(ud);
      gvDummy.RewindCheck();
      RUN_TEST_REQ(gvDummy, f9sv_Result_NoError == , f9sv_GridView, static_cast<int16_t>(10));
      gvDummy.Wait();
      puts("All tests have passed.");
      // ============================
      if (argc > 1 && strcmp(argv[1], "-w") == 0) {
         puts("Enter 'quit' to exit test.");
         char  cmdbuf[1024];
         while (fon9_AppBreakMsg == NULL) {
            if (!fgets(cmdbuf, sizeof(cmdbuf), stdin))
               break;
            fon9::StrView cmdln{fon9::StrView_cstr(cmdbuf)};
            if (fon9::StrTrim(&cmdln).empty())
               continue;
            if (cmdln == "quit")
               break;
         }
      }
      // ----------------------------
      f9rc_DestroyClientSession_Wait(ud.Session_);
   }
   fon9_Finalize();
}
