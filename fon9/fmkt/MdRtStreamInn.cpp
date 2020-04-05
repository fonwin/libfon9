// \file fon9/fmkt/MdRtStreamInn.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtStreamInn.hpp"
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/TimedFileName.hpp"
#include "fon9/Log.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 { namespace fmkt {

MdRtSubr::~MdRtSubr() {
}
//--------------------------------------------------------------------------//
MdRtStreamInnMgr::MdRtStreamInnMgr(MdSymbsBase& symbs, std::string rtiPathFmt)
   : RecoverThread_(rtiPathFmt.empty()
                    ? nullptr
                    : new TimerThread{RevPrintTo<std::string>("MdRtsInnMgr:", rtiPathFmt)})
   , MdSymbs_(symbs)
   , RtiPathFmt_{std::move(rtiPathFmt)} {
}
MdRtStreamInnMgr::~MdRtStreamInnMgr() {
   if (this->RecoverThread_)
      this->RecoverThread_->WaitForEndNow();
}
void MdRtStreamInnMgr::DailyClear(const unsigned tdayYYYYMMDD) {
   assert(this->TDayYYYYMMDD_ < tdayYYYYMMDD);
   this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   if (this->RtiPathFmt_.empty())
      return;

   TimedFileName logfn(this->RtiPathFmt_, TimedFileName::TimeScale::Day);
   logfn.RebuildFileNameYYYYMMDD(tdayYYYYMMDD);

   const std::string    logPath = logfn.GetFileName();
   InnApf::OpenArgs     oargs{logPath + ".rti"};
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
//--------------------------------------------------------------------------//
static inline bool IsStartTime(DayTime infoTime, DayTime reqTime, DayTime dailyClearTime) {
   if (infoTime.IsNull())
      return false;
   if (infoTime < dailyClearTime) { // 資料時間已跨日.
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

void MdRtRecover::OnTimer(TimeStamp now) {
   (void)now;
   if (this->RtSubr_->IsUnsubscribed())
      return;
   struct NotifyArgs : public seed::SeedNotifyStreamRecoverArgs {
      fon9_NON_COPY_NON_MOVE(NotifyArgs);
      using base = seed::SeedNotifyStreamRecoverArgs;
      using base::base;
      using base::CacheGV_;
      PosT  Pos_;
   };
   NotifyArgs  nargs(this->Mgr_.MdSymbs_, nullptr/*tab*/,
                     ToStrView(this->Reader_->Key()),
                     nullptr/*rd*/, seed::SeedNotifyKind::StreamRecover);

   // 在沒有流量管制的情況下,
   // 預設: this->RunAfter(TimeInterval_Millisecond(1)); 大約 1 秒 1000 次回補;
   // 所以一個回補要求, 沒流量管制時, 最大回補流量 = sizeof(rdbuf) * 1000 bytes / 每秒.
   char     rdbuf[4 * 1024];
   PosT     nextReadPos = nargs.Pos_ = (this->IsStarted_ ? this->LastPos_ : 0);
   size_t   bufofs = 0, errlen = 0;
   // - 首次執行(this->IsStarted_ == false), 則一定找到期望開始的位置為止.
   // - this->IsStarted_ == true: 還原上次未處理的資料量?
   //   => 通常每個封包不會太大, 在最後離開時, 就拋棄最後剩餘未處理的資料.
   //      => 下次重新讀取就好.
   //      => 如此可降低記憶體的需求.
   for (;;) {
      assert(this->EndPos_ >= nextReadPos);
      const size_t szToEnd = (this->EndPos_ - nextReadPos);
      if (szToEnd == 0) {
         assert(bufofs == 0);
         break;
      }
      size_t rdsz = sizeof(rdbuf) - bufofs;
      assert(rdsz > 0);
      if (rdsz > szToEnd)
         rdsz = szToEnd;
      if (this->RtSubr_->IsUnsubscribed())
         return;
      rdsz = this->Reader_->Read(nextReadPos, rdbuf + bufofs, rdsz);
      if (rdsz <= 0) {
         fon9_LOG_ERROR("MdRtStream.Read|key=", nargs.KeyText_,
                        "|err=Read 0"
                        "|pos=", nextReadPos);
         this->EndPos_ = nextReadPos;
         break;
      }
      nextReadPos += rdsz;

      const char* const rdend = rdbuf + bufofs + rdsz;
      DcQueueFixedMem   dcq{rdbuf, rdend};
      uint32_t          chkValue;
      const char*       pchk;
      while ((pchk = static_cast<const char*>(dcq.Peek(&chkValue, sizeof(chkValue)))) != nullptr) {
         chkValue = GetBigEndian<uint32_t>(pchk);
         const PosT currChkPos = (nextReadPos - (rdend - pchk));
         if (fon9_UNLIKELY(static_cast<uint32_t>(currChkPos) != chkValue)) {
            ++errlen;
            dcq.PopConsumed(1);
            continue;
         }
         if (fon9_UNLIKELY(errlen)) {
            fon9_LOG_ERROR("MdRtStream.Read|err=Bad storage"
                           "|pos=", currChkPos - errlen,
                           "|len=", errlen);
            errlen = 0;
         }
         dcq.PopConsumed(sizeof(chkValue));
         size_t pksz;
         if (!PopBitvByteArraySize(dcq, pksz))
            break;
         assert(pksz > 0);
         if (pksz <= 0)
            continue;
         // 檢查是否需要回補: pkRtsKind, infoTime.
         const MdRtsKind pkRtsKind = GetMdRtsKind(static_cast<RtsPackType>(*dcq.Peek1()));
         DcQueueFixedMem pk{dcq.Peek1() + 1, pksz - 1};
         BitvTo(pk, this->LastInfoTime_);
         if (!this->IsStarted_) {
            if (!IsStartTime(this->LastInfoTime_, this->StartInfoTime_, this->Mgr_.DailyClearTime()))
               goto __SKIP_PK;
            this->IsStarted_ = true;
         }
         if (!IsEnumContainsAny(this->Filter_, pkRtsKind))
            goto __SKIP_PK;

         nargs.StreamDataKind_ |= cast_to_underlying(pkRtsKind);
         if (nargs.CacheGV_.empty()) // 保留 pos, 若訂閱者流量管制, 則從 pos 開始.
            nargs.Pos_ = currChkPos;
         nargs.CacheGV_.append(pchk + sizeof(chkValue), reinterpret_cast<const char*>(dcq.Peek1() + pksz));
         // 回補通知. 多筆打包一次(增加回補效率).
         if (nargs.CacheGV_.size() > 1024) { // MTU = 1500?
            {  // lock tree: 確保可以安全的使用 this->RtSubr_;
               auto  treelk{this->Mgr_.MdSymbs_.SymbMap_.ConstLock()};
               if (this->RtSubr_->IsUnsubscribed())
                  return;
               this->RtSubr_(nargs);
            }  // unlock tree.
            if (fon9_UNLIKELY(!nargs.FlowControlWait_.IsNullOrZero())) {
               if (nargs.FlowControlWait_.GetOrigValue() < 0) {
                  // 強制流量管制, 結束此次回補, 設定下次回補時間.
                  nargs.FlowControlWait_.SetOrigValue(-nargs.FlowControlWait_.GetOrigValue());
                  this->LastPos_ = nargs.Pos_;
                  this->RunAfter(nargs.FlowControlWait_);
                  return;
               }
               // 返回 nargs.FlowControlWait_.GetOrigValue() > 0;
               // 不接受流量管制的建議, 從 Reader_ 讀入的資料, 必須優先處理完.
               // else {
               //    this->LastPos_ = currChkPos + (reinterpret_cast<const char*>(dcq.Peek1()) - pchk);
               //    this->RunAfter(nargs.FlowControlWait_);
               //    return;
               // }
            }
            nargs.CacheGV_.clear();
            nargs.StreamDataKind_ = 0;
            nargs.Pos_ = currChkPos;
         }

      __SKIP_PK:;
         dcq.PopConsumed(pksz);
      }
      bufofs = dcq.CalcSize();
      if (this->IsStarted_) {
         // 一旦已經找到開始位置, 且已處理了 block: sizeof(rdbuf);
         // 則暫時離開回補迴圈:
         // - 讓其他回補要求有機會執行.
         // - 降低大量回補造成單一物件(recover)的壓力, 讓 recover 有機會喘口氣.
         nextReadPos -= (pchk ? (bufofs + sizeof(chkValue)) : bufofs);
         this->LastPos_ = nextReadPos;
         break;
      }
      if (pchk)
         memmove(rdbuf, pchk, bufofs += sizeof(chkValue));
      else if (bufofs != 0)
         memmove(rdbuf, dcq.Peek1(), bufofs);
   }
   if (errlen) {
      fon9_LOG_ERROR("MdRtStream.Read|err=Bad storage"
                     "|pos=", nextReadPos - errlen,
                     "|len=", errlen);
   }
   // 回補完畢通知, 訂閱者需注意:
   // - e.CacheGV_ 可能還有一些「未使用 StreamRecover 通知」的回補訊息,
   //   在此會使用 StreamRecoverEnd 送出.
   // - 可參考 fon9/rc/RcMdRtsDecoder.cpp: DecodeStreamRecoverEnd() 的做法.
   { // lock tree: 確保可以安全的使用 this->RtSubr_;
      auto  treelk{this->Mgr_.MdSymbs_.SymbMap_.ConstLock()};
      if (this->RtSubr_->IsUnsubscribed())
         return;
      if (nextReadPos >= this->EndPos_)
         nargs.NotifyKind_ = seed::SeedNotifyKind::StreamRecoverEnd;
      else if (nargs.CacheGV_.empty()) {
         treelk.unlock();
         this->RunAfter(TimeInterval_Millisecond(1));
         return;
      }
      this->RtSubr_(nargs);
   } // unlock tree.

   if (fon9_LIKELY(nargs.FlowControlWait_.IsNullOrZero())) {
      if (nargs.NotifyKind_ != seed::SeedNotifyKind::StreamRecoverEnd)
         this->RunAfter(TimeInterval_Millisecond(1));
      return;
   }
   // 有流量管制, 設定下次回補時間.
   if (nargs.FlowControlWait_.GetOrigValue() < 0) {
      nargs.FlowControlWait_.SetOrigValue(-nargs.FlowControlWait_.GetOrigValue());
      this->LastPos_ = ((nargs.NotifyKind_ == seed::SeedNotifyKind::StreamRecoverEnd
                         && nargs.CacheGV_.empty())
                        ? this->EndPos_ : nargs.Pos_);
   }
   else { // 有流量管制, 但 nargs 的內容已處理.
      if (nargs.NotifyKind_ == seed::SeedNotifyKind::StreamRecoverEnd)
         return;
   }
   this->RunAfter(nargs.FlowControlWait_);
}

} } // namespaces
