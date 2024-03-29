﻿// \file fon9/rc/RcSvc.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSvc_hpp__
#define __fon9_rc_RcSvc_hpp__
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/seed/SeedAcl.hpp"
#include "fon9/ObjPool.hpp"
#include "fon9/Log.hpp"
#include <unordered_map>
#include <deque>

struct f9sv_Seed : public fon9::seed::Raw {
   fon9_NON_COPY_NON_MOVE(f9sv_Seed);
   f9sv_Seed() = default;
};

//-----------------------------
namespace fon9 { namespace rc {

class fon9_API RcSvStreamDecoder;
using RcSvStreamDecoderSP = std::unique_ptr<RcSvStreamDecoder>;

class fon9_API RcSvStreamDecoderNote;
using RcSvStreamDecoderNoteSP = std::unique_ptr<RcSvStreamDecoderNote>;

class RcSvcReq;
using RcSvcReqSP = std::unique_ptr<RcSvcReq>;

//-----------------------------
namespace svc {

struct SeedRec : public f9sv_Seed {
   fon9_NON_COPY_NON_MOVE(SeedRec);
   f9sv_ReportHandler      SeedHandler_;
   f9sv_SubrIndex          SubrIndex_{kSubrIndexNull};
   SvFuncCode              WorkingSvFunc_{};
   char                    Padding___[3];
   RcSvStreamDecoderNoteSP StreamDecoderNote_;

   SeedRec() {
      ZeroStruct(this->SeedHandler_);
   }
   // 一般功能(除訂閱外), 只要回覆一次, 所以在回覆前, 先清除 seed 的狀態.
   f9sv_FnOnReport OnBeforeReportToUser(f9sv_ClientReport& rpt, SeedRec& seed, SvFuncCode fcCode) {
      auto fnOnReport = seed.SeedHandler_.FnOnReport_;
      rpt.UserData_ = seed.SeedHandler_.UserData_;
      if (seed.WorkingSvFunc_ == fcCode || !fnOnReport) {
         this->SeedHandler_.FnOnReport_ = nullptr;
         this->WorkingSvFunc_ = SvFuncCode::Empty;
      }
      return fnOnReport;
   }
   void ClearSubscriber() {
      this->SubrIndex_ = kSubrIndexNull;
   }

   static SeedRec* MakeSeedRec(const seed::Tab& tab) {
      return seed::MakeDyMemRaw<SeedRec>(tab);
   }
};
using SeedSP = std::unique_ptr<SeedRec>;
using Seeds = std::vector<SeedSP>;

inline const f9sv_Seed** ToSeedArray(SeedSP* seedArray) {
   // 底下的這些 static_assert(), 是為了確定 C API 可以正確使用:
   //    struct f9sv_ClientReport 的 const struct f9sv_Seed** SeedArray_;
   static_assert(sizeof(svc::SeedSP) == sizeof(svc::SeedRec*), "");

   // ----- 只有 gcc 5 可以這樣用? MSVC 19, gcc 8 都不行?!
   #if defined(__GNUC__) && (__GNUC__ <= 5)
      static_assert(fon9_OffsetOfBase(SeedRec, f9sv_Seed) == 0, "");
   // enum { kOffset = fon9_OffsetOfBase(SeedRec, f9sv_Seed) }; static_assert(kOffset == 0, "");
   #else
   // ----- MSVC 19 (Release, Debug 都不行這樣用?!)
   // if (fon9_OffsetOfBase(svc::SeedRec, f9sv_Seed) != 0) {    // always false: compile 最佳化之後, 不會來到 if 裡面.
   //    extern void RcSv_SeedRec_Cannot_ConvertTo_f9sv_Seed(); // 所以如果有 link error,
   //    RcSv_SeedRec_Cannot_ConvertTo_f9sv_Seed();             // 則表示 svc::SeedRec* <=> f9sv_Seed* 無法直接轉換.
   // }
   // ----- 改判斷 SeedRec 的第1個 data member 的位置, 是否剛好緊接 f9sv_Seed 之後.
   fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
   static_assert(offsetof(SeedRec, SeedHandler_) == sizeof(f9sv_Seed) && sizeof(f9sv_Seed) != 0, "");
   fon9_GCC_WARN_POP;
   #endif
   // -----

   fon9_GCC_WARN_DISABLE("-Wold-style-cast");
   return (const f9sv_Seed**)(seedArray);
   fon9_GCC_WARN_POP;
}
// -----
struct PodRec {
   fon9_NON_COPY_NON_MOVE(PodRec);
   SeedSP* Seeds_{};// 在 TreeRec::FetchPod() 初始化.

   PodRec() = default;
   ~PodRec() {
      this->ClearSeeds();
   }
   void ClearSeeds() {
      if (this->Seeds_) {
         delete[] this->Seeds_;
         this->Seeds_ = nullptr;
      }
   }
};
using PodMap = std::unordered_map<CharVector, PodRec>;
// -----
/// 在收到 Layout 之前的查詢、訂閱, 都要先保留.
using PendingReqs = std::deque<RcSvcReqSP>;
// -----
struct TabRec {
   fon9_NON_COPYABLE(TabRec);
   TabRec() = default;
   TabRec(TabRec&&) = default;
   TabRec& operator=(TabRec&&) = default;
   std::vector<f9sv_Field> Fields_;
   RcSvStreamDecoderSP     StreamDecoder_;
};
using TabArray = std::vector<TabRec>;
// -----
struct TreeRec {
   fon9_NON_COPY_NON_MOVE(TreeRec);
   const CharVector  TreePath_;
   seed::LayoutSP    Layout_;
   f9sv_Layout       LayoutC_;     // 提供給 C API 使用 layout 的定義.
   TabArray          TabArray_;    // 提供給 C API 使用 LayoutC_.TabArray_ 裡面的 fields 的定義.
   PodMap            PodMap_;      // 此棵樹所擁有的 pods.
   PendingReqs       PendingReqs_; // 在尚未取得 layout 之前, 所有的要求暫放於此.

   TreeRec(StrView treePath) : TreePath_{treePath} {
      ZeroStruct(this->LayoutC_);
   }
   ~TreeRec();

   PodRec& FetchPod(StrView seedKey, bool* isNew = nullptr);
   void MakePod(PodRec& pod);
   void MakeSeed(SeedSP& seed, f9sv_TabSize tabidx);

   /// assert(this->Layout_.get() == nullptr);
   void ParseLayout(StrView cfgstr);
};
using TreeRecSP = std::unique_ptr<TreeRec>;
// -----
struct SubrRec {
   SvFuncCode           SubrSvFunc_{};
   bool                 IsStream_{false};
   char                 Padding___[2];
   f9sv_TabSize         TabIndex_{};

   TreeRec*             Tree_{};
   CharVector           SeedKey_;
   SeedSP*              Seeds_{}; // = PodRec::Seeds_

   f9sv_ReportHandler   SubrHandler_;

   SubrRec() {
      ZeroStruct(this->SubrHandler_);
   }

   void ClearSvFuncSubr() {
      this->SubrSvFunc_ = SvFuncCode{};
      this->Tree_       = nullptr;
      this->Seeds_      = nullptr;
      this->SubrHandler_.FnOnReport_ = nullptr;
   }

   /// 當有 layout 時, 訂閱記錄必須與 Pod(Seed) 建立關聯.
   void OnPodReady(const PodRec& pod, f9sv_SubrIndex subrIndex) {
      assert(this->Seeds_ == nullptr);
      assert(this->Tree_ && this->TabIndex_ < this->Tree_->LayoutC_.TabCount_);
      this->Seeds_ = pod.Seeds_;
      pod.Seeds_[this->TabIndex_]->SubrIndex_ = subrIndex;
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
//--------------------------------------------------------------------------//
struct RxSubrData {
   RxSubrData(RxSubrData&&) = delete;
   RxSubrData& operator=(RxSubrData&) = delete;
   RxSubrData& operator=(RxSubrData&&) = delete;

   f9rc_ClientSession&  Session_;
   seed::SeedNotifyKind NotifyKind_;
   const f9sv_SubrIndex SubrIndex_;
   SubrRec* const       SubrRec_;
   /// 必定為 SubrRec_->Seeds_[SubrRec_->TabIndex_];
   SeedRec* const       SeedRec_;
   /// 必定為 SubrRec_->Tree_->Layout_->GetTab(SubrRec_->TabIndex_);
   seed::Tab* const     SubrTab_;
   RevBufferList        LogBuf_{kLogBlockNodeSize};
   const TreeLocker     LockedMap_;
   /// 當 this->IsSubrTree_ 時, this->SeedKey_ 為主機端傳遞過來的 key;
   /// 當 !this->IsSubrTree_, 則 this->SeedKey_ = this->SubrRec_->SeedKey_;
   CharVector           SeedKey_;
   std::string          Gv_;
   bool                 IsSubrLogged_{false};
   bool                 IsNeedsLog_;
   const bool           IsSubrTree_;
   char                 Padding___[5];

   RxSubrData(f9rc_ClientSession& ses, SvFunc fcAck, TreeLocker&& maplk, DcQueue& rxbuf);
   /// src = StreamRecover; 建立 StreamData;
   /// - StreamRecover 可能包含多筆 StreamData;
   RxSubrData(RxSubrData& src);

   ~RxSubrData() {
      this->FlushLog();
   }
   void FlushLog();
   void LogSubrRec();
   StrView CheckLoadSeedKey(DcQueue& rxbuf);
   void LoadGv(DcQueue& rxbuf);

   /// 收到 ParentSeedClear, PodRemoved, SeedRemoved, StreamEnd.
   /// Server 端已移除訂閱, 此時應該移除 Client 訂閱記錄.
   void RemoveSubscribe();
};

} } } // namespaces
#endif//__fon9_rc_RcSvc_hpp__
