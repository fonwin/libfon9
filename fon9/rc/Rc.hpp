// \file fon9/rc/Rc.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_Rc_hpp__
#define __fon9_rc_Rc_hpp__
#include "fon9/buffer/DcQueue.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/intrusive_ptr.hpp"
#include "fon9/TimeStamp.hpp"
#include <array>
#include <memory>

namespace fon9 { namespace rc {

class fon9_API RcSession;
class fon9_API RcFunctionMgr;
class fon9_API RcFunctionAgent;
using RcFunctionAgentSP = std::unique_ptr<RcFunctionAgent>;

enum class RcFunctionCode : uint8_t {
   Connection = 0,
   SASL = 1,
   Logout = 2,
   Heartbeat = 3,

   SeedTree = 7,

   OmsApi = 8,

   /// 這裡提供一個暫時的最大值, 可減少一些記憶體用量.
   /// 目前規劃:
   /// - admin 功能(登入、Heartbeat...)
   /// - seed/tree
   /// - f9oms
   /// - 其他請參考 fon9/rc/README.md
   Max = 8,
};
constexpr unsigned GetRcFunctionCodeArraySize() {
   return cast_to_underlying(RcFunctionCode::Max) + 1u;
}

enum class RcSessionSt : int8_t {
   Closing = -1,
   DeviceLinkReady = 0,
   ProtocolReady,
   Connecting,
   Logoning,
   /// 登入成功後, 可開始進行 Ap 層的訊息交換(功能呼叫).
   ApReady,
};

/// \ingroup rc
/// 協調 FunctionAgent / RcSession 之間的關聯.
/// - 提供 FunctionAgent 的註冊及管理.
/// - RcSession 參考到一個 RcFunctionMgr.
/// - 建構時自動建立的 FunctionAgent: RcFunctionCode::Heartbeat, RcFunctionCode::Logout
class fon9_API RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(RcFunctionMgr);
   using FunctionAgents = std::array<RcFunctionAgentSP, GetRcFunctionCodeArraySize()>;
   FunctionAgents FunctionAgents_;
   void AddDefaultAgents();

   virtual unsigned AddRef() = 0;
   virtual unsigned Release() = 0;
public:
   /// 自動註冊 RcFunctionAgent: Logout, Heartbeat;
   RcFunctionMgr() {
      this->AddDefaultAgents();
   }
   virtual ~RcFunctionMgr();

   /// 註冊一個 FunctionAgent.
   /// \retval true  註冊成功.
   /// \retval false 該 fnCode 已有人先註冊了.
   bool Add(RcFunctionAgentSP&& agent);
   /// 強制註冊一個 FunctionAgent.
   /// 若已存在, 則取代之.
   void Reset(RcFunctionAgentSP&& agent);

   RcFunctionAgent* Get(RcFunctionCode fnCode) {
      return cast_to_underlying(fnCode) >= this->FunctionAgents_.size()
         ? nullptr : this->FunctionAgents_[cast_to_underlying(fnCode)].get();
   }

   void OnSessionLinkReady(RcSession&);
   void OnSessionProtocolReady(RcSession&);
   void OnSessionApReady(RcSession&);
   void OnSessionLinkBroken(RcSession&);

   inline friend unsigned intrusive_ptr_add_ref(const RcFunctionMgr* px) { return const_cast<RcFunctionMgr*>(px)->AddRef(); }
   inline friend unsigned intrusive_ptr_release(const RcFunctionMgr* px) { return const_cast<RcFunctionMgr*>(px)->Release(); }
};
using RcFunctionMgrSP = intrusive_ptr<RcFunctionMgr>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup rc
/// 當 RcSession 收到一個封包後,
/// 呼叫 RcFunctionAgent::OnRecvFunctionCall() 時, 提供此參數.
/// - 此時:
///   - this->RecvBuffer_ 的資料量必定 >= this->RemainParamSize_;
///   - 已經將 checksum、function code、param size 取出(移除).
/// - OnRecvFunctionCall() 返回前有義務將剩餘沒用到的參數移除:
///   - 取出參數時若有調整 this->RemainParamSize_,
///     則返回前可呼叫 this->RemoveRemainParam();
struct RcFunctionParam {
   fon9_NON_COPY_NON_MOVE(RcFunctionParam);

   DcQueue&    RecvBuffer_;
   size_t      RemainParamSize_;
   TimeStamp   RecvTime_{UtcNow()};

   RcFunctionParam(DcQueue& rxbuf, size_t remainParamSize = 0)
      : RecvBuffer_(rxbuf)
      , RemainParamSize_{remainParamSize} {
   }
   void RemoveRemainParam() {
      this->RecvBuffer_.PopConsumed(this->RemainParamSize_);
   }
};

/// \ingroup rc
/// 對應 RcFunctionCode 處理 f9rc 的功能.
class fon9_API RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFunctionAgent);
public:
   const RcFunctionCode FunctionCode_;
   const RcSessionSt    SessionStRequired_;
   RcFunctionAgent(RcFunctionCode fnCode, RcSessionSt stReq = RcSessionSt::ApReady)
      : FunctionCode_{fnCode}
      , SessionStRequired_{stReq} {
   }
   virtual ~RcFunctionAgent();

   /// 登入成功後通知: 預設 do nothing.
   virtual void OnSessionApReady(RcSession& ses);
   /// 斷線時通知, 預設自動清除 Session Note: ses.ResetNote(this->FunctionCode_, nullptr);
   virtual void OnSessionLinkBroken(RcSession& ses);

   /// 當 RcSession 沒找到對應的 RcFunctionNote 時, 呼叫此處進行處理.
   /// 如果有找到 RcFunctionNote, 就不會再呼叫此處了!
   virtual void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param);
};
fon9_WARN_POP;

/// \ingroup rc
/// RcFunctionAgent 可在 RcSession 裡面注入一個針對該 Function 所需的資料.
/// 此物件由 RcSession 所擁有.
class fon9_API RcFunctionNote {
public:
   virtual ~RcFunctionNote();
   /// 當 RcSession 有對應的 Note, 則會直接進行呼叫, 不會檢查 RcFunctionAgent::SessionStRequired_,
   /// 也 **不會** 再呼叫 RcFunctionAgent::OnRecvFunctionCall();
   virtual void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) = 0;
};
using RcFunctionNoteSP = std::unique_ptr<RcFunctionNote>;

} } // namespaces
#endif//__fon9_rc_Rc_hpp__
