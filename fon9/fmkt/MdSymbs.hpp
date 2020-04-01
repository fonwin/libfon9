// \file fon9/fmkt/MdSymbs.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdSymbs_hpp__
#define __fon9_fmkt_MdSymbs_hpp__
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/fmkt/MdRtStream.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace fon9 { namespace fmkt {

template <class MdSymbT>
inline auto IsSymbExpired(const MdSymbT& symb, unsigned tdayYYYYMMDD)
-> decltype(symb.EndYYYYMMDD_ != 0) {
   return(symb.EndYYYYMMDD_ != 0 && symb.EndYYYYMMDD_ < tdayYYYYMMDD);
}

template <class MdSymbT>
inline auto IsSymbExpired(const MdSymbT& symb, unsigned tdayYYYYMMDD)
-> decltype(symb.IsSymbExpired(tdayYYYYMMDD)) {
   return symb.IsSymbExpired(tdayYYYYMMDD);
}

inline bool IsSymbExpired(...) {
   return false;
}

//--------------------------------------------------------------------------//

/// MdSymbs 提供:
/// - 協助訂閱即時資料.
/// - 協助使用 RtInnMgr_ 提供儲存即時資料.
/// - 協助 DailyClear;
/// - MdSymbT 必須提供:
///   - DailyClear(fon9::fmkt::SymbTree& tree, unsigned tdayYYYYMMDD);
///   - BeforeRemove(fon9::fmkt::SymbTree& tree, unsigned tdayYYYYMMDD);
///   - fon9::fmkt::MdRtStream  MdRtStream_;
template <class MdSymbT>
class MdSymbs : public SymbTree {
   fon9_NON_COPY_NON_MOVE(MdSymbs);
   using base = SymbTree;

public:
   seed::Tab* const  RtTab_;
   MdRtStreamInnMgr  RtInnMgr_;

   fon9_MSC_WARN_DISABLE(4355) // 'this': used in base member initializer list
   MdSymbs(seed::LayoutSP layout, std::string rtiPathFmt)
      : base{std::move(layout)}
      , RtTab_{LayoutSP_->GetTab(fon9_kCSTR_TabName_Rt)}
      , RtInnMgr_(*this, std::move(rtiPathFmt)) {
   }
   fon9_MSC_WARN_POP;

   void SetDailyClearHHMMSS(unsigned hhmmss) {
      this->RtInnMgr_.SetDailyClearHHMMSS(hhmmss);
   }
   void DailyClear(unsigned tdayYYYYMMDD) {
      auto symbs = this->SymbMap_.Lock();
      this->RtInnMgr_.DailyClear(tdayYYYYMMDD);
      auto iend = symbs->end();
      for (auto ibeg = symbs->begin(); ibeg != iend;) {
         auto& symb = *static_cast<MdSymbT*>(ibeg->second.get());
         if (!IsSymbExpired(std::ref(symb), tdayYYYYMMDD)) {
            symb.DailyClear(*this, tdayYYYYMMDD);
            ++ibeg;
         }
         else { // 移除已下市商品, 移除時需觸發 PodRemoved 事件.
            symb.BeforeRemove(*this, tdayYYYYMMDD);
            ibeg = symbs->erase(ibeg);
         }
      }
   }
   void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override {
      using namespace fon9::seed;
      struct SymOp : public PodOp {
         fon9_NON_COPY_NON_MOVE(SymOp);
         using base = PodOp;
         using base::base;
         OpResult SubscribeStream(SubConn* pSubConn, Tab& tab, StrView args, FnSeedSubr subr) override {
            if (static_cast<MdSymbs*>(this->Sender_)->RtTab_ != &tab)
               return SubscribeStreamUnsupported(pSubConn);
            return static_cast<MdSymbT*>(this->Symb_.get())->MdRtStream_.SubscribeStream(
               this->LockedMap_, pSubConn, tab, *this, args, std::move(subr));
         }
         OpResult UnsubscribeStream(SubConn subConn, Tab& tab) override {
            if (static_cast<MdSymbs*>(this->Sender_)->RtTab_ != &tab)
               return OpResult::not_supported_subscribe_stream;
            return static_cast<MdSymbT*>(this->Symb_.get())->MdRtStream_.UnsubscribeStream(
               this->LockedMap_, subConn);
         }
      };
      struct Op : public TreeOp {
         fon9_NON_COPY_NON_MOVE(Op);
         using TreeOp::TreeOp;
         void OnSymbPodOp(const StrView& strKeyText, fmkt::SymbSP&& symb, FnPodOp&& fnCallback, const Locker& lockedMap) override {
            SymOp op(this->Tree_, strKeyText, std::move(symb), lockedMap);
            fnCallback(op, &op);
         }
      } op{*this};
      fnCallback(TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
   }
};

} } // namespaces
#endif//__fon9_fmkt_MdSymbs_hpp__
