// \file fon9/fmkt/MdRtStreamInn.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtStreamInn.hpp"
#include "fon9/TimedFileName.hpp"
#include "fon9/Log.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 { namespace fmkt {

enum class RecoverResult {
   RecoverContinue,
   RecoverEnd,
   Unsubscribed,
};
struct MdRtStreamInnMgr::MdRtRecoverHandler {
   fon9_NON_COPY_NON_MOVE(MdRtRecoverHandler);
   using MessageType = MdRtRecover;
   MdRtStreamInnMgr& Owner_;
   MdRtRecoverHandler(MdRtRecoverPool& pool)
      : Owner_(ContainerOf(pool, &MdRtStreamInnMgr::RecoverPool_)) {
   }
   void OnThreadEnd(const std::string& threadName) {
      (void)threadName;
   }

   void OnMessage(MdRtRecover& recover) {
      if (recover.RtSubr_->IsUnsubscribed())
         return;
      switch (this->OnRecover(recover)) {
      case RecoverResult::RecoverContinue:
         this->Owner_.RecoverPool_.EmplaceMessage(std::move(recover));
         break;
      case RecoverResult::RecoverEnd:
      case RecoverResult::Unsubscribed:
         break;
      }
   }
   RecoverResult OnRecover(MdRtRecover& recover);
};

static inline bool IsStartTime(DayTime infoTime, DayTime reqTime, DayTime dailyClearTime) {
   if (infoTime.IsNull())
      return false;
   if (infoTime < dailyClearTime) {   // 資料時間已跨日.
      if (reqTime < dailyClearTime) // 回補要求,也是跨日.
         return infoTime >= reqTime;
      // 回補要求沒有跨日.
      return true;
   }
   // 資料時間(infoTime)沒有跨日.
   if (reqTime < dailyClearTime) // 回補要求,是跨日.
      return false;
   // 資料時間(infoTime)沒有跨日, 回補要求也沒有跨日.
   return infoTime >= reqTime;
}
RecoverResult MdRtStreamInnMgr::MdRtRecoverHandler::OnRecover(MdRtRecover& recover) {
   struct NotifyArgs : public seed::SeedNotifyArgs {
      fon9_NON_COPY_NON_MOVE(NotifyArgs);
      using base = seed::SeedNotifyArgs;
      using base::base;
      using base::CacheGV_;
   };
   NotifyArgs  e(this->Owner_.SymbTree_, nullptr/*tab*/,
                 ToStrView(recover.Reader_->Key()),
                 nullptr/*rd*/, seed::SeedNotifyKind::StreamRecover);

   char     buf[1024 * 64];
   uint64_t pos = recover.IsStarted_ ? recover.LastPos_ : 0;
   size_t   bufofs = 0, errlen = 0;
   // recover.IsStarted_ == true: 還原最後未處理的資料量?
   // => 通常每個封包不會太大, 在最後離開時, 就拋棄最後剩餘未處理的資料.
   //    => 下次重新讀取就好.
   //    => 如此可降低記憶體的需求.
   for (;;) {
      assert(recover.EndPos_ >= pos);
      const size_t esz = (recover.EndPos_ - pos);
      if (esz == 0) {
         assert(bufofs == 0);
         break;
      }
      size_t rdsz = sizeof(buf) - bufofs;
      assert(rdsz > 0);
      if (rdsz > esz)
         rdsz = esz;
      rdsz = recover.Reader_->Read(pos, buf + bufofs, rdsz);
      if (rdsz <= 0) {
         fon9_LOG_ERROR("MdRtStream.Read|key=", e.KeyText_,
                        "|err=Read 0"
                        "|pos=", pos);
         recover.EndPos_ = pos;
         break;
      }
      pos += rdsz;

      const char* const pend = buf + bufofs + rdsz;
      DcQueueFixedMem   dcq{buf, pend};
      uint32_t          chkpos;
      const char*       pchk;
      while ((pchk = static_cast<const char*>(dcq.Peek(&chkpos, sizeof(chkpos)))) != nullptr) {
         chkpos = GetBigEndian<uint32_t>(pchk);
         if (fon9_UNLIKELY(static_cast<uint32_t>(pos - (pend - pchk)) != chkpos)) {
            ++errlen;
            dcq.PopConsumed(1);
            continue;
         }
         if (fon9_UNLIKELY(errlen)) {
            fon9_LOG_ERROR("MdRtStream.Read|err=Bad storage"
                           "|pos=", pos - (pend - pchk) - errlen,
                           "|len=", errlen);
            errlen = 0;
         }
         dcq.PopConsumed(sizeof(chkpos));
         size_t pksz;
         if (!PopBitvByteArraySize(dcq, pksz))
            break;
         assert(pksz > 0);
         if (pksz <= 0)
            continue;
         // 檢查是否需要回補: pkRtsKind, infoTime.
         const MdRtsKind pkRtsKind = GetMdRtsKind(static_cast<RtsPackType>(*dcq.Peek1()));
         DcQueueFixedMem pk{dcq.Peek1() + 1, pksz - 1};
         BitvTo(pk, recover.LastInfoTime_);
         if (!recover.IsStarted_) {
            if (!IsStartTime(recover.LastInfoTime_, recover.StartInfoTime_, this->Owner_.DailyClearTime_))
               goto __SKIP_PK;
            recover.IsStarted_ = true;
         }
         if (!IsEnumContainsAny(recover.Filter_, pkRtsKind))
            goto __SKIP_PK;

         e.StreamDataKind_ |= cast_to_underlying(pkRtsKind);
         e.CacheGV_.append(pchk + sizeof(chkpos), reinterpret_cast<const char*>(dcq.Peek1() + pksz));
         // 回補通知. 多筆打包一次(增加回補效率).
         if (e.CacheGV_.size() > 1024) { // MTU = 1500?
            auto  treelk{this->Owner_.SymbTree_.SymbMap_.ConstLock()};
            if (recover.RtSubr_->IsUnsubscribed())
               return RecoverResult::Unsubscribed;
            recover.RtSubr_(e);
            e.CacheGV_.clear();
            e.StreamDataKind_ = 0;
         }

      __SKIP_PK:;
         dcq.PopConsumed(pksz);
      }
      bufofs = dcq.CalcSize();
      if (recover.IsStarted_) {
         pos -= (pchk ? (bufofs + sizeof(chkpos)) : bufofs);
         recover.LastPos_ = pos;
         break;
      }
      if (pchk)
         memmove(buf, pchk, bufofs += sizeof(chkpos));
      else if (bufofs != 0)
         memmove(buf, dcq.Peek1(), bufofs);
   }
   if (errlen) {
      fon9_LOG_ERROR("MdRtStream.Read|err=Bad storage"
                     "|pos=", pos - errlen,
                     "|len=", errlen);
   }
   // 回補完畢通知, 訂閱者需注意:
   // - e.CacheGV_ 可能還有一些「未使用 StreamRecover 通知」的回補訊息,
   //   在此會使用 StreamRecoverEnd 送出.
   // - 可參考 fon9/rc/RcMdRtsDecoder.cpp: DecodeStreamRecoverEnd() 的做法.
   auto  treelk{this->Owner_.SymbTree_.SymbMap_.ConstLock()};
   if (recover.RtSubr_->IsUnsubscribed())
      return RecoverResult::Unsubscribed;
   if (pos >= recover.EndPos_)
      e.NotifyKind_ = seed::SeedNotifyKind::StreamRecoverEnd;
   else if (e.CacheGV_.empty())
      return RecoverResult::RecoverContinue;

   recover.RtSubr_(e);
   return e.NotifyKind_ == seed::SeedNotifyKind::StreamRecoverEnd
      ? RecoverResult::RecoverEnd : RecoverResult::RecoverContinue;
}
//--------------------------------------------------------------------------//
MdRtStreamInnMgr::MdRtStreamInnMgr(SymbTree& symbTree, std::string pathFmt)
   : SymbTree_(symbTree)
   , PathFmt_{std::move(pathFmt)} {
   this->RecoverPool_.StartThread(1,
      ToStrView(RevPrintTo<std::string>("MdRtStreamInnMgr|path=", this->PathFmt_)));
}
MdRtStreamInnMgr::~MdRtStreamInnMgr() {
   this->RecoverPool_.WaitForEndNow();
}
void MdRtStreamInnMgr::DailyClear(unsigned tdayYYYYMMDD) {
   assert(this->TDayYYYYMMDD_ < tdayYYYYMMDD);
   this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   TimedFileName logfn(this->PathFmt_, TimedFileName::TimeScale::Day);
   // 檔名與 TDay 相關, 與 TimeZone 無關, 所以要扣除 logfn.GetTimeChecker().GetTimeZoneOffset();
   logfn.RebuildFileName(YYYYMMDDHHMMSS_ToTimeStamp(tdayYYYYMMDD, 0) - logfn.GetTimeChecker().GetTimeZoneOffset());
   const std::string logPath = logfn.GetFileName();

   InnApf::OpenArgs     oargs{logPath + ".rt"};
   InnStream::OpenArgs  sargs{InnRoomType{}};
   InnApf::OpenResult   res{};
   sargs.ExpectedRoomSize_[0] = 1024 * 2;
   sargs.ExpectedRoomSize_[1] = 1024 * 4;
   sargs.ExpectedRoomSize_[2] = 1024 * 8;
   sargs.ExpectedRoomSize_[3] = 1024 * 16;
   this->RtInn_ = InnApf::Make(oargs, sargs, res);
   if (!this->RtInn_)
      fon9_LOG_FATAL("MdRtStreamInnMgr.DailyClear|fname=", oargs.FileName_, "|tday=", tdayYYYYMMDD, '|', res);
}
InnApf::OpenResult MdRtStreamInnMgr::RtOpen(InnApf::StreamRW& rw, const Symb& symb) {
   assert(symb.TDayYYYYMMDD_ == this->TDayYYYYMMDD_);
   if (this->RtInn_) {
      if (rw.Owner() == this->RtInn_.get())
         return InnApf::OpenResult{};
      return rw.Open(*this->RtInn_, ToStrView(symb.SymbId_),
                     FileMode::Append | FileMode::Read | FileMode::OpenAlways);
   }
   rw.Close();
   return InnApf::OpenResult{std::errc::bad_file_descriptor};
}
void MdRtStreamInnMgr::AddRecover(MdRtRecover&& recover) {
   if (fon9_LIKELY(recover.Reader_.get()))
      this->RecoverPool_.EmplaceMessage(std::move(recover));
   else {
      // TODO: Stream Recover 失敗訊息? (沒有開啟 reader, 無法回補.)
      seed::SeedNotifyArgs  e(this->SymbTree_, nullptr/*tab*/,
                              ToStrView(recover.Reader_->Key()),
                              nullptr/*rd*/, seed::SeedNotifyKind::StreamRecoverEnd);
      recover.RtSubr_(e);
   }
}

} } // namespaces
