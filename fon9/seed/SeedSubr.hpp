/// \file fon9/seed/SeedSubr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_SeedSubr_hpp__
#define __fon9_seed_SeedSubr_hpp__
#include "fon9/seed/Tab.hpp"
#include "fon9/seed/RawRd.hpp"
#include "fon9/Subr.hpp"

namespace fon9 { namespace seed {

fon9_WARN_DISABLE_PADDING;
class fon9_API SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(SeedNotifyArgs);
protected:
   std::string    mutable CacheGV_;
   virtual void MakeGridView() const;

public:
   enum class NotifyType {
      /// 訂閱成功返回前的通知.
      /// 若在訂閱成功後, 有立即取得資料的需求, 則需處理此通知. 
      SubscribeOK = 0,
      SeedChanged = 1,
      PodRemoved  = 2,
      SeedRemoved = 3,
      /// 整個資料表的異動, 例如 Apply 之後的通知.
      TableChanged = 4,
      /// OnParentSeedClear() 的事件通知; 收到此通知之後, 訂閱會被清除, 不會再收到事件.
      ParentSeedClear = 5,
   };
   const NotifyType  NotifyType_;

   Tree&          Tree_;
   /// NotifyType::PodRemoved 此處為 nullptr.
   Tab*           Tab_;
   /// - IsTextBegin(args.KeyText_): OnParentSeedClear() 的事件通知.
   /// - IsTextEnd(args.KeyText_):   整個資料表的異動, 例如: Apply 之後的通知.
   const StrView  KeyText_;
   /// - 不為 nullptr:
   ///   - NotifyType::SeedChanged
   ///   - 訂閱 seed 時的 NotifyType::SubscribeOK, 且 seed 有資料.
   /// - 其他情況則為 nullptr
   const RawRd*   Rd_;

   SeedNotifyArgs(Tree& tree, Tab* tab, const StrView& keyText, const RawRd* rd, NotifyType type)
      : NotifyType_(type), Tree_(tree), Tab_(tab), KeyText_(keyText), Rd_(rd) {
   }
   virtual ~SeedNotifyArgs();

   /// - NotifyType::SeedChanged:
   ///   - 預設: 不含 this->KeyText_, 僅包含 Tab_.Fields_, 頭尾不含分隔字元,
   ///     也就是使用: FieldsCellRevPrint0NoSpl();
   ///   - 若為特殊的資料, 則由事件觸發者自訂, 訂閱者必須明瞭自己訂閱的資料格式.
   /// - NotifyType::TableChanged:
   ///   - 預設是 TreeOp::GridView(GridViewRequestFull{});
   const std::string& GetGridView() const {
      if (this->CacheGV_.empty())
         this->MakeGridView();
      return this->CacheGV_;
   }
};
fon9_WARN_POP;

class fon9_API SeedNotifySubscribeOK : public SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(SeedNotifySubscribeOK);
   using base = SeedNotifyArgs;
protected:
   void MakeGridView() const override;
public:
   TreeOp&  OpTree_;
   SeedNotifySubscribeOK(TreeOp& opTree, Tab& tab, const StrView& keyText, const RawRd* rd);
};

using FnSeedSubr = std::function<void(const SeedNotifyArgs&)>;

class UnsafeSeedSubj : public UnsafeSubject<FnSeedSubr> {
   fon9_NON_COPY_NON_MOVE(UnsafeSeedSubj);
   using base = UnsafeSubject<FnSeedSubr>;
public:
   using base::base;
   UnsafeSeedSubj() = default;

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
      return this->Unsubscribe(subConn) ? OpResult::no_error : OpResult::not_supported_subscribe;
   }
};

fon9_API void SeedSubj_NotifyPodRemoved(UnsafeSeedSubj& subj, Tree& tree, StrView keyText);
fon9_API void SeedSubj_NotifySeedRemoved(UnsafeSeedSubj& subj, Tree& tree, Tab& tab, StrView keyText);
fon9_API void SeedSubj_ParentSeedClear(UnsafeSeedSubj& subj, Tree& tree);
fon9_API void SeedSubj_TableChanged(UnsafeSeedSubj& subj, Tree& tree, Tab& tab);
fon9_API void SeedSubj_Notify(UnsafeSeedSubj& subj, const SeedNotifyArgs& args);

template <class Rec>
inline void SeedSubj_Notify(UnsafeSeedSubj& subj, Tree& tree, Tab& tab, StrView keyText, const Rec& rec) {
   if (!subj.IsEmpty()) {
      SimpleRawRd rd{rec};
      SeedSubj_Notify(subj, SeedNotifyArgs(tree, &tab, keyText, &rd, SeedNotifyArgs::NotifyType::SeedChanged));
   }
}

} } // namespaces
#endif//__fon9_seed_SeedSubr_hpp__
