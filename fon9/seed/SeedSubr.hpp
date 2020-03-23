/// \file fon9/seed/SeedSubr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_SeedSubr_hpp__
#define __fon9_seed_SeedSubr_hpp__
#include "fon9/seed/Tab.hpp"
#include "fon9/seed/RawRd.hpp"
#include "fon9/Subr.hpp"
#include "fon9/TimeInterval.hpp"

namespace fon9 { namespace seed {

enum class SeedNotifyKind {
   /// 訂閱成功返回前的通知.
   /// 若在訂閱成功後, 有立即取得資料的需求, 則需處理此通知. 
   SubscribeOK = 0,
   SeedChanged = 1,
   PodRemoved = 2,
   SeedRemoved = 3,
   /// 整個資料表的異動, 例如 Apply 之後的通知.
   TableChanged = 4,
   /// OnParentSeedClear() 的事件通知; 收到此通知之後, 訂閱會被清除, 不會再收到事件.
   ParentSeedClear = 5,

   /// SubscribableOp::SubscribeStream() 訂閱成功時的通知.
   /// - 此時 GetGridView() 返回的是自訂格式的內容.
   SubscribeStreamOK = 6,
   /// 由「Stream 發行者」提供的自訂格式.
   /// - 此時 GetGridView() 返回的是自訂格式的內容.
   /// - 必須透過 SubscribableOp::SubscribeStream() 來訂閱, 才會有此事件.
   StreamData = 7,
   /// Stream 回補訊息, 由訂閱者與發行者自行協商內容.
   StreamRecover = 8,
   /// Stream 回補結束訊息, 由訂閱者與發行者自行協商內容.
   /// 如果回補完後, Stream 已確定不會再有訊息,
   /// 則會直接回覆 StreamEnd, 不會有 StreamRecoverEnd;
   StreamRecoverEnd = 9,
   /// Stream 已結束, 已主動移除訂閱, 不會再收到 Stream 的訊息.
   StreamEnd = 10,
};

fon9_WARN_DISABLE_PADDING;
class fon9_API SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(SeedNotifyArgs);
protected:
   std::string    mutable CacheGV_;
   virtual void MakeGridView() const;

public:
   SeedNotifyKind NotifyKind_;
   Tree&          Tree_;
   /// - 若為 NotifyKind = PodRemoved 則此處為 nullptr.
   /// - 若為 NotifyKind = StreamData 則由 Stream 的提供者決定.
   ///   - 例: fon9::fmkt::MdRtStream 的通知, 此處為 nullptr;
   ///     因為 GetGridView() 為自訂格式,
   ///     可提供 Pod 裡面任意一個 Seed 的內容, 提供 Tab_ 意義不大.
   Tab* const     Tab_;
   /// - IsTextBegin(args.KeyText_): ParentSeedClear 的事件通知.
   /// - IsTextEnd(args.KeyText_):   TableChanged 的事件通知, 例如: Apply 之後的通知.
   const StrView  KeyText_;
   /// - 不為 nullptr:
   ///   - NotifyKind = SeedChanged
   ///   - 訂閱 seed 時的 NotifyKind = SubscribeOK, 且 seed 有資料.
   /// - 其他情況則為 nullptr
   const RawRd* const   Rd_{};
   /// NotifyKind = StreamData 事件, 提供額外的參數,
   /// 讓訂閱者可以依此判斷此次的 Stream 所提供的內容.
   /// 由「Stream 發行者」解釋此欄位, 例: fon9/fmkt/MdRtsTypes.hpp: enum class MdRtsKind.
   uintmax_t            StreamDataKind_{};

   SeedNotifyArgs(Tree& tree, Tab* tab, const StrView& keyText, const RawRd* rd, SeedNotifyKind kind)
      : NotifyKind_(kind), Tree_(tree), Tab_(tab), KeyText_(keyText), Rd_(rd) {
   }

   template <typename StreamDataKind,
      enable_if_t<std::is_enum<StreamDataKind>::value, StreamDataKind> = StreamDataKind{}>
   SeedNotifyArgs(Tree& tree, Tab* tab, const StrView& keyText, StreamDataKind streamDataKind)
      : SeedNotifyArgs(tree, tab, keyText, cast_to_underlying(streamDataKind)) {
   }
   SeedNotifyArgs(Tree& tree, Tab* tab, const StrView& keyText, uintmax_t streamDataKind)
      : NotifyKind_(SeedNotifyKind::StreamData), Tree_(tree), Tab_(tab), KeyText_(keyText)
      , StreamDataKind_(streamDataKind) {
   }

   virtual ~SeedNotifyArgs();

   /// - NotifyKind = SeedChanged:
   ///   - 預設: 不含 this->KeyText_, 僅包含 Tab_.Fields_, 頭尾不含分隔字元,
   ///     也就是使用: FieldsCellRevPrint0NoSpl();
   ///   - 若為特殊的資料, 則由事件觸發者自訂, 訂閱者必須明瞭自己訂閱的資料格式.
   /// - NotifyKind = TableChanged:
   ///   - 預設是 TreeOp::GridView(GridViewRequestFull{});
   const std::string& GetGridView() const {
      if (this->CacheGV_.empty())
         this->MakeGridView();
      return this->CacheGV_;
   }
};
fon9_WARN_POP;

/// 一般訂閱成功時的通知.
/// 不含 Stream 訂閱成功, Stream 訂閱成功, 由 Stream 提供者自行處理.
class fon9_API SeedNotifySubscribeOK : public SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(SeedNotifySubscribeOK);
   using base = SeedNotifyArgs;
protected:
   void MakeGridView() const override;
public:
   TreeOp&  OpTree_;
   SeedNotifySubscribeOK(TreeOp& opTree, Tab& tab, const StrView& keyText, const RawRd* rd);
};

/// SeedNotifyKind::StreamRecover, SeedNotifyKind::StreamRecoverEnd 的通知.
/// 訂閱者可返回因「流量管制」需要延遲的時間.
/// 訂閱者可直接使用 static_cast<SeedNotifyStreamRecoverArgs*>(&e) 轉型.
class fon9_API SeedNotifyStreamRecoverArgs : public SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(SeedNotifyStreamRecoverArgs);
   using base = SeedNotifyArgs;
public:
   /// 訂閱者返回前, 可設定此值, 告知發行者流量管制.
   /// - <0 表示此次的回補無法處理, 必須延遲 (-FlowWait_) 之後再嘗試.
   ///      發行者應「必須配合」此返回值來管制流量.
   /// - >0 表示此次的回補已處理, 但需要先暫停回補, 延遲 (FlowWait_) 之後再嘗試.
   ///      發行者應「盡量配合」此返回值來管制流量,
   ///      但沒有「約束」效果, 發行者可以不遵守。
   mutable TimeInterval FlowControlWait_;

   using base::base;
};

using FnSeedSubr = std::function<void(const SeedNotifyArgs&)>;

template <class FnSubrT>
class UnsafeSeedSubjT : public UnsafeSubject<FnSubrT> {
   fon9_NON_COPY_NON_MOVE(UnsafeSeedSubjT);
   using base = UnsafeSubject<FnSubrT>;
public:
   using base::base;
   UnsafeSeedSubjT() = default;

   template<class Locker>
   OpResult SafeSubscribe(const Locker& lk, TreeOp& opTree, SubConn* pSubConn, Tab& tab, const FnSeedSubr& subr) {
      (void)lk; assert(lk.owns_lock());
      this->Subscribe(pSubConn, subr);
      subr(SeedNotifySubscribeOK{opTree, tab, TextBegin(), nullptr});
      return OpResult::no_error;
   }
   template<class Locker>
   OpResult SafeUnsubscribe(const Locker& lk, SubConn subConn) {
      (void)lk; assert(lk.owns_lock());
      return this->Unsubscribe(subConn) ? OpResult::no_error : OpResult::no_subscribe;
   }
};
using UnsafeSeedSubj = UnsafeSeedSubjT<FnSeedSubr>;

fon9_API void SeedSubj_NotifyPodRemoved(UnsafeSeedSubj& subj, Tree& tree, StrView keyText);
fon9_API void SeedSubj_NotifySeedRemoved(UnsafeSeedSubj& subj, Tree& tree, Tab& tab, StrView keyText);
fon9_API void SeedSubj_ParentSeedClear(UnsafeSeedSubj& subj, Tree& tree);
fon9_API void SeedSubj_TableChanged(UnsafeSeedSubj& subj, Tree& tree, Tab& tab);
fon9_API void SeedSubj_Notify(UnsafeSeedSubj& subj, const SeedNotifyArgs& args);

template <class Subj, class Rec>
inline void SeedSubj_Notify(Subj& subj, Tree& tree, Tab& tab, StrView keyText, const Rec& rec) {
   if (!subj.IsEmpty()) {
      SimpleRawRd rd{rec};
      SeedSubj_Notify(subj, SeedNotifyArgs(tree, &tab, keyText, &rd, SeedNotifyKind::SeedChanged));
   }
}

} } // namespaces
#endif//__fon9_seed_SeedSubr_hpp__
