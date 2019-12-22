// \file fon9/rc/RcSeedVisitorClient.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorClient_hpp__
#define __fon9_rc_RcSeedVisitorClient_hpp__
#include "fon9/rc/RcClientSession.hpp"
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/seed/SeedAcl.hpp"
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
   FlowCounterThreadSafe            FcQry_;

   RcSeedVisitorClientNote(const f9sv_ClientSessionParams& params) : Params_(params) {
   }
   ~RcSeedVisitorClientNote();

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;

private:
   friend class RcSeedVisitorClientAgent;
   void OnSessionLinkBroken(RcClientSession& ses);
   void ClearRequests(RcClientSession& ses);
   void OnRecvAclConfig(RcClientSession& ses, RcFunctionParam& param);
   void OnRecvQueryAck(RcClientSession& ses, DcQueue& rxbuf, SvFuncCode fcAck);

   /// 提供給 fon9::rc::SvParseGvTables() 解析使用的訊息, 分隔符號變成 EOS.
   std::string          ConfigGvTablesStr_;
   SvGvTables           ConfigGvTables_;
   f9sv_ClientConfig    Config_;
   std::string          OrigGvTablesStr_;
   seed::AclConfig      Ac_;
   // -----
   struct SeedRec : public f9sv_Seed {
      fon9_NON_COPYABLE(SeedRec);
      SeedRec() {
         ZeroStruct(this->Handler_);
      }
      SvFuncCode         CurrSt_{};
      char               Padding___[7];
      f9sv_ReportHandler Handler_;
      static SeedRec* MakeSeedRec(const seed::Tab& tab) {
         return seed::MakeDyMemRaw<SeedRec>(tab);
      }
   };
   using SeedSP = std::unique_ptr<SeedRec>;
   using Seeds = std::vector<SeedSP>;
   // -----
   struct PodRec {
      fon9_NON_COPY_NON_MOVE(PodRec);
      SeedSP* Seeds_{};
      PodRec() = default;
      ~PodRec() {
         delete [] this->Seeds_;
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
      SvFuncCode           SvFunc_;
      char                 Padding___[3];
      void HandlerToSeed(SeedRec& seed) const {
         seed.Handler_ = this->Handler_;
         seed.CurrSt_ = this->SvFunc_;
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
      fon9_NON_COPYABLE(TreeRec);
      f9sv_Layout    LayoutC_;
      seed::LayoutSP Layout_;
      TabList        TabList_;
      PodMap         PodMap_;
      PendingReqs    PendingReqs_;

      TreeRec() {
         ZeroStruct(this->LayoutC_);
      }
      TreeRec(TreeRec&&) = default;
      TreeRec& operator=(TreeRec&&) = default;

      PodRec& FetchPod(StrView seedKey, bool* isNew = nullptr);
   };
   using TreeMapImpl = seed::AclPathMap<TreeRec>;
   using TreeMap = MustLock<TreeMapImpl, std::recursive_mutex>;
   TreeMap  TreeMap_;
   friend struct RcSvClientRequest;
   struct RejectRequest;

   void ParseLayout(RcClientSession& ses, TreeMapImpl::value_type& itree, DcQueue& rxbuf);
};

} } // namespaces
#endif//__fon9_rc_RcSeedVisitorClient_hpp__
