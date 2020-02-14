// \file fon9/rc/RcSeedVisitorClient.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorClient_hpp__
#define __fon9_rc_RcSeedVisitorClient_hpp__
#include "fon9/rc/RcClientSession.hpp"
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/seed/SeedAcl.hpp"
#include "fon9/ObjPool.hpp"
#include <unordered_map>
#include <deque>

extern "C" {
   struct f9sv_Seed : public fon9::seed::Raw {
      fon9_NON_COPY_NON_MOVE(f9sv_Seed);
      f9sv_Seed() = default;
   };
};

namespace fon9 { namespace rc {

/// \ingroup rc
/// FunctionAgent: SeedVisitor Client.
/// ApReady 後, 送出權限查詢訊息, 取得可 tree list 的相關權限.
class RcSeedVisitorClientAgent : public RcClientFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcSeedVisitorClientAgent);
   using base = RcClientFunctionAgent;
public:
   RcSeedVisitorClientAgent() : base{f9rc_FunctionCode_SeedVisitor} {
   }
   ~RcSeedVisitorClientAgent();

   void OnSessionCtor(RcClientSession& ses, const f9rc_ClientSessionParams* params);
   void OnSessionApReady(RcSession& ses) override;
   void OnSessionLinkBroken(RcSession& ses) override;
};

struct RcSvClientRequest;

class RcSeedVisitorClientNote : public RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(RcSeedVisitorClientNote);
public:
   const f9sv_ClientSessionParams   Params_;

   RcSeedVisitorClientNote(const f9sv_ClientSessionParams& params) : Params_(params) {
   }
   ~RcSeedVisitorClientNote();

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;

   /// 在 RcSeedVisitorClientAgent::OnSessionLinkBroken() 轉呼叫此處.
   void OnSessionLinkBroken(RcClientSession& ses);
   /// 在 RcSeedVisitorClientAgent::OnSessionApReady() 轉呼叫此處.
   void ClearRequests(RcClientSession& ses);

   // -----
   struct SeedRec : public f9sv_Seed {
      fon9_NON_COPYABLE(SeedRec);
      SeedRec() {
         ZeroStruct(this->Handler_);
      }
      f9sv_ReportHandler Handler_;
      f9sv_SubrIndex     SubrIndex_{kSubrIndexNull};
      SvFunc             SvFunc_{};
      char               Padding___[3];
      void ClearSvFunc() {
         this->Handler_.FnOnReport_ = nullptr;
         this->SubrIndex_ = kSubrIndexNull;
         this->SvFunc_ = SvFunc::Empty;
      }

      static SeedRec* MakeSeedRec(const seed::Tab& tab) {
         return seed::MakeDyMemRaw<SeedRec>(tab);
      }
   };
   using SeedSP = std::unique_ptr<SeedRec>;
   using Seeds = std::vector<SeedSP>;
   // -----
   struct PodRec {
      fon9_NON_COPY_NON_MOVE(PodRec);
      SeedSP* Seeds_{};// 在 TreeRec::FetchPod() 初始化.

      PodRec() = default;
      ~PodRec() {
         delete[] this->Seeds_;
      }
   };
   using PodMap = std::unordered_map<CharVector, PodRec>;
   // -----
   /// 在收到 Layout 之前的查詢、訂閱, 都要先保留.
   struct PendingReq {
      f9sv_ReportHandler   Handler_;
      CharVector           SeedKey_;
      CharVector           TabName_;
      f9sv_TabSize         TabIndex_;
      f9sv_SubrIndex       SubrIndex_;
      SvFunc               SvFunc_;
      char                 Padding___[7];
      void AssignToSeed(SeedRec& seed) const {
         seed.Handler_   = this->Handler_;
         seed.SvFunc_    = this->SvFunc_;
         seed.SubrIndex_ = this->SubrIndex_;
      }
   };
   using PendingReqs = std::deque<PendingReq>;
   // -----
   struct TabC : public f9sv_Tab {
      std::vector<f9sv_Field> Fields_;
   };
   using TabList = std::vector<TabC>;
   // -----
   struct TreeRec {
      fon9_NON_COPY_NON_MOVE(TreeRec);
      const CharVector  TreePath_;
      seed::LayoutSP    Layout_;
      f9sv_Layout       LayoutC_;     // 提供給 C API 使用 layout 的定義.
      TabList           TabList_;     // 提供給 C API 使用 LayoutC_ 裡面的 tab 的定義.
      PodMap            PodMap_;      // 此棵樹所擁有的 pods.
      PendingReqs       PendingReqs_; // 在尚未取得 layout 之前, 所有的要求暫放於此.

      TreeRec(StrView treePath) : TreePath_{treePath} {
         ZeroStruct(this->LayoutC_);
      }
      PodRec& FetchPod(StrView seedKey, bool* isNew = nullptr);
   };
   using TreeRecSP = std::unique_ptr<TreeRec>;
   // -----
   struct SubrRec {
      TreeRec*       Tree_{};
      CharVector     SeedKey_;
      SeedSP*        Seeds_{}; // = PodRec::Seeds_
      f9sv_TabSize   TabIndex_{};
      /// 設定成 SvFuncCode::Unsubscribe 之後, 要拋棄所有的 [訂閱、訂閱資料] 事件.
      SvFuncCode     SvFunc_{};
      char           Padding___[3];
      void ClearSvFunc() {
         this->Tree_ = nullptr;
         this->Seeds_ = nullptr;
         this->SvFunc_ = SvFuncCode{};
      }
   };
   using SubrMap = ObjPool<SubrRec>;
   // -----
   class TreeMapImpl : public seed::AclPathMap<TreeRecSP> {
      using base = seed::AclPathMap<TreeRecSP>;
   public:
      /// 訂閱註記, 使用 f9sv_SubrIndex 當作索引.
      /// - 訂閱要求時 Alloc().
      /// - 收到取消確認時才會呼叫: RemoveObj();
      SubrMap  SubrMap_{64}; // reserveSize = 64;

      void clear() {
         base::clear();
         this->SubrMap_.MoveOut();
      }
      TreeRec& fetch(const StrView& treePath) {
         auto& itree = this->kfetch(CharVector::MakeRef(treePath));
         if (!itree.second)
            itree.second.reset(new TreeRec{treePath});
         return *itree.second;
      }
   };
   /// 在事件通知時, 允許使用者呼叫「f9sv_ API」, 所以使用 recursive_mutex;
   using TreeMap = MustLock<TreeMapImpl, std::recursive_mutex>;
   using TreeLocker = TreeMap::Locker;
   TreeLocker LockTree() {
      return this->TreeMap_.Lock();
   }
   // -----
   TimeInterval FcQryFetch(const TreeLocker& locker) {
      (void)locker; assert(locker.owns_lock());
      return this->FcQry_.Fetch();
   }
   f9sv_Result AddSubr(const TreeLocker& locker, RcSvClientRequest& req, const PodRec* pod);

private:
   struct RejectRequest;

   void OnRecvAclConfig(RcClientSession& ses, RcFunctionParam& param);
   void OnRecvQrySubrAck(RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck, StrView fnName);
   void OnRecvUnsubrAck(RcClientSession& ses, DcQueue& rxbuf);
   void OnRecvSubrData(RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck);
   void ParseLayoutAndSendPendings(const TreeLocker& locker, RcClientSession& ses, TreeRec& tree, DcQueue& rxbuf);

   // ConfigGvTablesStr_: 提供給 fon9::rc::SvParseGvTables() 解析使用的訊息, 分隔符號變成 EOS.
   std::string          ConfigGvTablesStr_;
   SvGvTables           ConfigGvTables_;
   f9sv_ClientConfig    Config_;
   std::string          OrigGvTablesStr_;
   seed::AclConfig      Ac_;
   FlowCounter          FcQry_;
   TreeMap              TreeMap_;
};

} } // namespaces
#endif//__fon9_rc_RcSeedVisitorClient_hpp__
