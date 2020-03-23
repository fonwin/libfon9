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
   SymbTree&            SymbTree_;
   const std::string    PathFmt_;

   /// - pathFmt = 儲存檔名的路徑格式(不含副檔名),
   ///   使用 fon9::TimedFileName 格式;
   MdRtStreamInnMgr(SymbTree& symbTree, std::string pathFmt);
   ~MdRtStreamInnMgr();

   unsigned TDayYYYYMMDD() const {
      return this->TDayYYYYMMDD_;
   }
   void DailyClear(unsigned tdayYYYYMMDD);

   /// dailyClearTime 用來檢查回補時間為今日 or 跨日.
   void SetDailyClearTime(DayTime dailyClearTime) {
      this->DailyClearTime_ = dailyClearTime;
   }
   DayTime DailyClearTime() const {
      return this->DailyClearTime_;
   }

   InnApf::OpenResult RtOpen(InnApf::StreamRW& rw, const Symb& symb);
};

struct MdRtSubr : public intrusive_ref_counter<MdRtSubr> {
   MdRtsKind         RtFilter_{MdRtsKind::All};
   seed::FnSeedSubr  Callback_;
   MdRtSubr(seed::FnSeedSubr&& sub) : Callback_{std::move(sub)} {
   }
   bool IsUnsubscribed() const {
      return this->RtFilter_ == MdRtsKind{};
   }
   void SetUnsubscribed() {
      this->RtFilter_ = MdRtsKind{};
   }
};
struct MdRtSubrSP : public intrusive_ptr<MdRtSubr> {
   using base = intrusive_ptr<MdRtSubr>;
   using base::base;
   /// 送出前必須 lock tree, 檢查 IsUnsubscribed();
   void operator()(const seed::SeedNotifyArgs& e) const {
      assert(static_cast<SymbTree*>(&e.Tree_)->SymbMap_.IsLocked());
      assert(!this->get()->IsUnsubscribed());
      this->get()->Callback_(e);
   }
};
struct MdRtRecover : public TimerEntry {
   fon9_NON_COPY_NON_MOVE(MdRtRecover);
   using PosT = InnApf::SizeT;
   using StreamSP = InnApf::StreamSP;
   MdRtStreamInnMgr& Mgr_;
   const MdRtSubrSP  RtSubr_;
   const StreamSP    Reader_;
   MdRtsKind         Filter_{};
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
