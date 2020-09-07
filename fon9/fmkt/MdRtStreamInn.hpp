// \file fon9/fmkt/MdRtStreamInn.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdRtStreamInn_hpp__
#define __fon9_fmkt_MdRtStreamInn_hpp__
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/InnApf.hpp"
#include "fon9/Timer.hpp"

namespace fon9 { namespace fmkt {

class fon9_API MdSymbsBase;

// MdRts 封包儲存到 Inn 時.
// 在封包前端放 4 bytes(uint32_t, big endian) = 實際的位置, 當成檢查碼.
// 當檔案有異常時, 可以用此找到下一個正確的位置.
using MdRtStreamInn_ChkValueType = uint32_t;

/// MdRtStream 的儲存機制;
/// - 即時儲存.
/// - 歷史回補.
/// - 所有的操作 symb tree 必定處在 lock 狀態, 所以這裡不再額外 lock.
class fon9_API MdRtStreamInnMgr {
   fon9_NON_COPY_NON_MOVE(MdRtStreamInnMgr);
   unsigned TDayYYYYMMDD_{};
   char     Padding______[4];
   InnApfSP RtInn_;
   DayTime  DailyClearTime_;

public:
   const TimerThreadSP  RecoverThread_;
   MdSymbsBase&         MdSymbs_;
   const std::string    RtiPathFmt_;

   /// - rtiPathFmt = 儲存檔名的路徑格式(不含副檔名, 開檔時自動加上 .rti),
   ///   使用 fon9::TimedFileName 格式;
   /// - 若 rtiPathFmt.empty() 表示不儲存;
   MdRtStreamInnMgr(MdSymbsBase& symbs, std::string rtiPathFmt);
   ~MdRtStreamInnMgr();

   unsigned TDayYYYYMMDD() const {
      return this->TDayYYYYMMDD_;
   }
   void DailyClear(unsigned tdayYYYYMMDD);

   /// hhmmss = DailyClearTime 用來檢查回補時間為今日 or 跨日.
   void SetDailyClearHHMMSS(unsigned hhmmss) {
      this->DailyClearTime_ = TimeInterval_HHMMSS(hhmmss);
   }
   DayTime DailyClearTime() const {
      return this->DailyClearTime_;
   }

   InnApf::OpenResult RtOpen(InnApf::StreamRW& rw, const Symb& symb);
};
//--------------------------------------------------------------------------//
struct fon9_API MdRtSubr : public intrusive_ref_counter<MdRtSubr> {
   f9sv_MdRtsKind    RtFilter_{f9sv_MdRtsKind_Full};
   seed::FnSeedSubr  Callback_;
   /// 從 args 取出 RtFiller_ = f9sv_MdRtsKind(hex): empty() or 0 表示訂閱全部的即時訊息;
   /// 並移動 args->begin() 到後面的參數位置.
   MdRtSubr(seed::FnSeedSubr&& sub, StrView* args)
      : RtFilter_{StrToMdRtsKind(args)}
      , Callback_{std::move(sub)} {
   }
   virtual ~MdRtSubr();

   bool IsUnsubscribed() const {
      return this->RtFilter_ == f9sv_MdRtsKind{};
   }
   void SetUnsubscribed() {
      this->RtFilter_ = f9sv_MdRtsKind{};
   }
};
struct MdRtSubrSP : public intrusive_ptr<MdRtSubr> {
   using base = intrusive_ptr<MdRtSubr>;
   using base::base;
   /// 呼叫前必須: lock tree, 檢查 IsUnsubscribed();
   void operator()(const seed::SeedNotifyArgs& e) const {
      assert(static_cast<SymbTree*>(&e.Tree_)->SymbMap_.IsLocked());
      assert(!this->get()->IsUnsubscribed());
      if (IsEnumContainsAny(this->get()->RtFilter_, static_cast<f9sv_MdRtsKind>(e.StreamDataKind_)))
         this->get()->Callback_(e);
   }
};
using MdRtUnsafeSubj = seed::UnsafeSeedSubjT<MdRtSubrSP>;

template <class UnsafeSubj>
inline seed::OpResult MdRtUnsafeSubj_UnsubscribeStream(UnsafeSubj& subj, SubConn subConn) {
   typename UnsafeSubj::Subscriber psub;
   if (!subj.MoveOutSubscriber(subConn, psub))
      return seed::OpResult::no_subscribe;
   assert(psub.get() != nullptr && psub->IsUnsubscribed() == false);
   psub->SetUnsubscribed(); // 告知回補程序: 已經取消訂閱, 不用再處理回補了!
   return seed::OpResult::no_error;
}

//--------------------------------------------------------------------------//
struct MdRtRecover : public TimerEntry {
   fon9_NON_COPY_NON_MOVE(MdRtRecover);
   using PosT = InnApf::SizeT;
   using StreamSP = InnApf::StreamSP;
   MdRtStreamInnMgr& Mgr_;
   const MdRtSubrSP  RtSubr_;
   const StreamSP    Reader_;
   f9sv_MdRtsKind    Filter_{};
   char              Padding___[3];
   bool              IsStarted_{false};

   /// IsStarted_ == false 使用 StartInfoTime_;
   /// IsStarted_ == true 使用 LastPos_;
   union {
      DayTime  StartInfoTime_;
      PosT     LastPos_;
   };

   /// 保留從 StreamInn 取出的最後 InfoTime,
   /// 因為若 InfoTime 使用 Bitv Null 儲存, 表示該時間與「上次」相同,
   /// 所以必須保留「上次的InfoTime」.
   DayTime  LastInfoTime_{DayTime::Null()};

   /// 訂閱時要求回補, 則回補到「訂閱時的最後位置」為止,
   /// 因為從該位置開始有即時通知,
   /// 訂閱者必須能處理「回補與即時」交錯回報的情況.
   PosT  EndPos_{};

   using MdRtRecoverSP = intrusive_ptr<MdRtRecover>;
   inline static MdRtRecoverSP Make(MdRtStreamInnMgr& mgr, MdRtSubrSP subr, StreamSP reader) {
      if (reader.get() == nullptr)
         return nullptr;
      return new MdRtRecover(mgr, std::move(subr), std::move(reader));
   }

   void SetStartInfoTime(DayTime tm) {
      if (tm.IsNull()) {
         this->IsStarted_ = true;
         this->LastPos_ = 0;
      }
      else {
         this->IsStarted_ = false;
         this->StartInfoTime_ = tm;
      }
   }

private:
   fon9_MSC_WARN_DISABLE(4582) // 'StartInfoTime_' : constructor is not implicitly called
   MdRtRecover(MdRtStreamInnMgr& mgr, MdRtSubrSP subr, StreamSP reader)
      : TimerEntry{mgr.RecoverThread_}
      , Mgr_(mgr)
      , RtSubr_{std::move(subr)}
      , Reader_{std::move(reader)} {
      assert(this->RtSubr_.get() != nullptr && this->Reader_.get() != nullptr);
   }
   fon9_MSC_WARN_POP;

   void OnTimer(TimeStamp now) override;
};
using MdRtRecoverSP = intrusive_ptr<MdRtRecover>;

} } // namespaces
#endif//__fon9_fmkt_MdRtStreamInn_hpp__
