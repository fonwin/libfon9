// \file fon9/fmkt/MdRtStreamInn.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdRtStreamInn_hpp__
#define __fon9_fmkt_MdRtStreamInn_hpp__
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/InnApf.hpp"
#include "fon9/MessageQueue.hpp"

namespace fon9 { namespace fmkt {

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
   void operator()(const seed::SeedNotifyArgs& e) {
      assert(static_cast<SymbTree*>(&e.Tree_)->SymbMap_.IsLocked());
      assert(!this->get()->IsUnsubscribed());
      this->get()->Callback_(e);
   }
};
struct MdRtRecover {
   MdRtSubrSP        RtSubr_;
   InnApf::StreamSP  Reader_;
   MdRtsKind         Filter_{};
   char              Padding___[3];
   bool              IsStarted_{false};
   union {
      // IsStarted_ == false 使用 StartTime_;
      DayTime        StartInfoTime_;
      // IsStarted_ == true 使用 LastPos_;
      InnApf::SizeT  LastPos_;
   };
   DayTime           LastInfoTime_{DayTime::Null()};
   InnApf::SizeT     EndPos_{};

   fon9_MSC_WARN_DISABLE(4582) // 'StartTime_' : constructor is not implicitly called
   MdRtRecover(MdRtSubrSP subr, InnApf::StreamSP reader)
      : RtSubr_{std::move(subr)}
      , Reader_{std::move(reader)} {
   }
   fon9_MSC_WARN_POP;
};

/// MdRtStream 的儲存機制;
/// - 即時儲存.
/// - 歷史回補.
/// - 所有的操作 symb tree 必定處在 lock 狀態, 所以這裡不再額外 lock.
class fon9_API MdRtStreamInnMgr {
   fon9_NON_COPY_NON_MOVE(MdRtStreamInnMgr);
   unsigned TDayYYYYMMDD_{};
   char     Padding______[4];
   InnApfSP RtInn_;

   struct MdRtRecoverHandler;
   using MdRtRecoverPool = MessageQueue<MdRtRecoverHandler, MdRtRecover>;
   MdRtRecoverPool   RecoverPool_;
   DayTime           DailyClearTime_;

public:
   SymbTree&         SymbTree_;
   const std::string PathFmt_;

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

   InnApf::OpenResult RtOpen(InnApf::StreamRW& rw, const Symb& symb);

   void AddRecover(MdRtRecover&&);
};

} } // namespaces
#endif//__fon9_fmkt_MdRtStreamInn_hpp__
