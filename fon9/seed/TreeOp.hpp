﻿/// \file fon9/seed/TreeOp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_TreeOp_hpp__
#define __fon9_seed_TreeOp_hpp__
#include "fon9/seed/SeedSubr.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/StrVref.hpp"

namespace fon9 { namespace seed {
fon9_WARN_DISABLE_PADDING;

//--------------------------------------------------------------------------//

struct PodOpResult {
   /// 此值必定有效, 不會是 nullptr.
   Tree*    Sender_;
   OpResult OpResult_;
   StrView  KeyText_;

   PodOpResult(Tree& sender, OpResult res, const StrView& key)
      : Sender_{&sender}, OpResult_{res}, KeyText_{key} {
   }
   PodOpResult(OpResult res, const StrView& key)
      : Sender_{nullptr}, OpResult_{res}, KeyText_{key} {
   }
};

class fon9_API PodOp;
using FnPodOp = std::function<void(const PodOpResult& res, PodOp* op)>;

struct PodRemoveResult : public PodOpResult {
   PodRemoveResult(Tree& sender, OpResult res, const StrView& key, Tab* tab)
      : PodOpResult{sender, res, key}
      , Tab_{tab} {
   }
   PodRemoveResult(OpResult res, const StrView& key)
      : PodOpResult{res, key}
      , Tab_{nullptr} {
   }
   /// 若為 nullptr 表示 Remove Pod 的結果.
   /// 若為 !nullptr 表示 Remove Seed 的結果.
   Tab*     Tab_;
};
using FnPodRemoved = std::function<void(const PodRemoveResult& res)>;

//--------------------------------------------------------------------------//

struct SeedOpResult : public PodOpResult {
   SeedOpResult(Tree& sender, OpResult res, const StrView& key, Tab* tab = nullptr)
      : PodOpResult{sender, res, key}
      , Tab_{tab} {
   }
   /// - 在 FnCommandResultHandler() 裡面:
   ///   - 若為 nullptr 表示 Pod Command 的結果.
   ///   - 若為 !nullptr 表示 Seed Command的結果.
   /// - 在 FnReadOp() 或 FnWriteOp()
   ///   - 此值必定有效, 不會是 nullptr.
   Tab*     Tab_;
   /// 若此處非 null, 則取代預設的 log, 改用此處紀錄 log;
   /// 參考 TicketRunnerCommand::OnSeedCommandResult();
   /// 通常用於: 寫 log 時, 排除一些機密資訊(例:密碼);
   StrView  LogStr_{nullptr};
};

using FnReadOp = std::function<void(const SeedOpResult& res, const RawRd* rd)>;
using FnWriteOp = std::function<void(const SeedOpResult& res, const RawWr* wr)>;
using FnCommandResultHandler = std::function<void(const SeedOpResult& res, StrView msg)>;

//--------------------------------------------------------------------------//

template <class Container>
static auto ContainerLowerBound(Container& container, StrView strKeyText) -> decltype(container.lower_bound(strKeyText)) {
   return container.lower_bound(strKeyText);
}

template <class Container>
static auto ContainerFind(Container& container, StrView strKeyText) -> decltype(container.find(strKeyText)) {
   return container.find(strKeyText);
}

template <class Container>
using ContainerIterator = conditional_t<std::is_const<Container>::value,
   typename Container::const_iterator,
   typename Container::iterator>;

//--------------------------------------------------------------------------//

struct GridViewRequest {
   /// 在 TreeOp::GetGridView() 實作時要注意:
   /// - 如果要切到另一 thread 處理:
   ///   若 OrigKey_ 不是 TreeOp::kStrKeyText_Begin_; 或 TreeOp::kStrKeyText_End_;
   ///   則 OrigKey_ **必須** 要用 std::string 複製一份.
   StrView  OrigKey_{nullptr};

   /// nullptr: 只取出 Key.
   /// !nullptr: 指定 Tab.
   Tab*     Tab_{nullptr};

   /// 用 ifind = lower_bound(OrigKey_) 之後.
   /// ifind 移動幾步之後才開始抓取資料;
   int16_t  Offset_{0};

   /// 最多取出幾筆資料, 0 = 不限制(由實作者自行決定).
   uint16_t MaxRowCount_{0};

   /// GridViewResult::GridView_ 最多最大容量限制, 0則表示不限制.
   /// 實際取出的資料量可能 >= MaxBufferSize_;
   uint32_t MaxBufferSize_{10 * 1024};

   GridViewRequest(const StrView& origKey) : OrigKey_{origKey} {
   }
};
struct GridViewRequestFull : public GridViewRequest {
   GridViewRequestFull(Tab& tab) : GridViewRequest{TextBegin()} {
      this->Tab_ = &tab;
      this->MaxBufferSize_ = 0;
   }
};
struct GridViewResult {
   Tree*       Sender_;
   Tab*        Tab_;
   OpResult    OpResult_;

   /// 使用 kRowSplitter 分隔行, 使用 kCellSplitter 分隔 cell.
   std::string GridView_;
   /// 存於 GridView_ 的最後一筆 key str.
   StrVref     LastKey_;

   /// GridView_ 裡面有幾個行(Row)資料?
   uint16_t    RowCount_{0};

   /// GridView_ 裡面第一行距離 begin 多遠?
   /// 如果不支援計算距離, 則為 GridViewResult::kNotSupported;
   size_t      DistanceBegin_{kNotSupported};

   /// GridView_ 裡面最後行距離 end 多遠?
   /// 1 表示 this->GridView_ 裡面的最後一筆 == Container 裡面的最後一筆.
   /// 如果不支援計算距離, 則為 GridViewResult::kNotSupported;
   size_t      DistanceEnd_{kNotSupported};

   /// Container 共有幾筆資料.
   size_t      ContainerSize_{kNotSupported};

   GridViewResult(Tree& sender, Tab* tab, OpResult res = OpResult::no_error)
      : Sender_(&sender)
      , Tab_(tab)
      , OpResult_{res} {
   }
   GridViewResult(OpResult res) : Sender_{nullptr}, Tab_{nullptr}, OpResult_{res} {
   }

   enum : size_t {
      /// 如果 DistanceBegin_, DistanceEnd_ 不支援, 則等於此值.
      kNotSupported = static_cast<size_t>(-1)
   };
   enum : char {
      kCellSplitter = *fon9_kCSTR_CELLSPL,
      kRowSplitter = *fon9_kCSTR_ROWSPL,
      /// 用在 "^apply:tab" 分隔修改前後的 gv:
      /// 若改前、改後資料不同, 則返回:
      /// - 改前 gv + kRowSplitter
      /// - kApplySplitter + "SubmitId=" + IdForApplySubmit
      /// - 改前 gv
      kApplySplitter = '\f',
   };

   template <class Container>
   auto SetContainerSize(const Container* container) -> decltype(container->size(), void()) {
      this->ContainerSize_ = container->size();
   }
   void SetContainerSize(...) {
      this->ContainerSize_ = kNotSupported;
   }

   void SetLastKey(size_t lastLinePos) {
      std::string::size_type spl = this->GridView_.find(kCellSplitter, lastLinePos);
      this->LastKey_.SetPosSize(lastLinePos,
         (spl == std::string::npos ? this->GridView_.size() : spl) - lastLinePos);
   }
   StrView GetLastKey() const {
      return this->LastKey_.ToStrView(this->GridView_.c_str());
   }
};
using FnGridViewOp = std::function<void(GridViewResult& result)>;

struct GridApplySubmitRequest {
   Tab*     Tab_;
   /// 要 ApplySubmit 的資料表(GridView).
   StrView  EditingGrid_;
   /// 若 !BeforeGrid_.IsNull(),
   /// 則在套用前必須檢查:「現在gv」== BeforeGrid_ 才能執行套用.
   StrView  BeforeGrid_;
};

//--------------------------------------------------------------------------//

template <class Container, class Iterator>
static bool GetIteratorForGv(Container& container, Iterator& istart, const char* strKeyText) {
   if (strKeyText == kStrKeyText_Begin_)
      istart = container.begin();
   else if (IsTextEnd(strKeyText))
      istart = container.end();
   else
      return false;
   return true;
}

template <class Container, class Iterator = ContainerIterator<Container> >
static Iterator GetIteratorForGv(Container& container, StrView strKeyText) {
   Iterator ivalue;
   if (GetIteratorForGv(container, ivalue, strKeyText.begin()))
      return ivalue;
   return ContainerLowerBound(container, strKeyText);
}

template <class Container, class Iterator = ContainerIterator<Container>>
static Iterator GetIteratorForPod(Container& container, StrView strKeyText) {
   if (strKeyText.begin() == kStrKeyText_Begin_)
      return container.begin();
   return ContainerFind(container, strKeyText);
}

//--------------------------------------------------------------------------//

class fon9_API SubscribableOp {
   fon9_NON_COPY_NON_MOVE(SubscribableOp);
protected:
   virtual ~SubscribableOp();

public:
   SubscribableOp() = default;

   /// 額外檢查是否有權限訂閱.
   /// 來到這裡之前必定已經先檢查過 ACL;
   /// 預設傳回 OpResult::no_error;
   virtual OpResult CheckSubscribeRights(Tab& tab, const SeedVisitor& from);

   /// 訂閱異動通知.
   virtual OpResult Subscribe(SubConn* pSubConn, Tab& tab, FnSeedSubr subr);
   /// 取消異動通知.
   virtual OpResult Unsubscribe(SubConn* pSubConn, Tab& tab);
   OpResult UnsubscribeUnsafe(SubConn subConn, Tab& tab) {
      return this->Unsubscribe(&subConn, tab);
   }

   /// 訂閱 Stream 通知.
   virtual OpResult SubscribeStream(SubConn* pSubConn, Tab& tab, StrView args, FnSeedSubr subr);
   /// 取消 Stream 通知.
   virtual OpResult UnsubscribeStream(SubConn* pSubConn, Tab& tab);
   OpResult UnsubscribeStreamUnsafe(SubConn subConn, Tab& tab) {
      return this->UnsubscribeStream(&subConn, tab);
   }
};

inline OpResult SubscribeUnsupported(SubConn* pSubConn) {
   *pSubConn = nullptr;
   return OpResult::not_supported_subscribe;
}
inline OpResult SubscribeStreamUnsupported(SubConn* pSubConn) {
   *pSubConn = nullptr;
   return OpResult::not_supported_subscribe_stream;
}

/// \ingroup seed
/// Tree 的(管理)操作不是放在 class Tree's methods? 因為:
/// - 無法在操作前知道如何安全的操作 tree:
///   - mutex lock? AQueue? or 切到指定的 thread? 還是用其他方法?
///   - 只有 tree 自己知道何時何地才能安全的操作.
///   - 所以使用 callback(FnTreeOp) 的方式, 提供操作 tree 的物件: TreeOp.
/// - 必須透過 Tree::OnTreeOp(FnOnTreeOp fnCallback);
///   - 然後在 fnCallback(OpResult res, TreeOp* op) 裡面,
///   - 對 op 進行操作(thread safe).
class fon9_API TreeOp : public SubscribableOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
protected:
   virtual ~TreeOp();

public:
   Tree& Tree_;
   TreeOp(Tree& tree) : Tree_(tree) {
   }

   virtual void GridView(const GridViewRequest& req, FnGridViewOp fnCallback);
   virtual void GridApplySubmit(const GridApplySubmitRequest& req, FnCommandResultHandler fnCallback);

   /// 增加一個 pod.
   /// - 當 key 存在時, 視為成功, 會呼叫: fnCallback(OpResult::key_exists, op);
   /// - 有些 tree 不允許從管理介面加入 pod, 此時會呼叫: fnCallback(OpResult::not_supported_add_pod, nullptr);
   /// - 若 tree 有支援 append(), 則 strKeyText 可使用 TreeOp::TextEnd();
   virtual void Add(StrView strKeyText, FnPodOp fnCallback);
   virtual void Get(StrView strKeyText, FnPodOp fnCallback);

   /// - strKeyText 不可為 TextBegin(), TextEnd();
   /// - tab == nullptr: 移除 pod.
   /// - tab != nullptr
   ///   - 可能僅移除某 seed.
   ///   - 或將該 seed 資料清除(變成初始狀態).
   ///   - 或不支援.
   ///   - 由實作者決定.
   virtual void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback);
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// \ingroup seed
/// - 若 tab != nullptr 則 not_supported_remove_seed
/// - 若 tab == nullptr 則 container.find() & container.erase();
template <class Container, class Key = typename Container::key_type>
void RemovePod(Tree& sender, Container& container, const Key& key, StrView strKeyText, Tab* tab, FnPodRemoved&& fnCallback) {
   PodRemoveResult res{sender, OpResult::not_supported_remove_seed, strKeyText, tab};
   if (tab == nullptr) {
      auto  i = container.find(key);
      if (i == container.end())
         res.OpResult_ = OpResult::not_found_key;
      else {
         container.erase(i);
         res.OpResult_ = OpResult::removed_pod;
      }
   }
   fnCallback(res);
}

/// \ingroup seed
/// 協助 TreeOp::Get().
/// TreeT 必須提供 TreeT::PodOp;
template <class TreeT, class Container, class Iterator = typename Container::iterator>
void GetPod(TreeT& sender, Container& container, Iterator ivalue, StrView strKeyText, FnPodOp&& fnCallback) {
   if (ivalue == container.end())
      fnCallback(PodOpResult{sender, OpResult::not_found_key, strKeyText}, nullptr);
   else {
      typename TreeT::PodOp op{*ivalue, sender, OpResult::no_error, strKeyText};
      fnCallback(op, &op);
   }
}

/// \ingroup seed
/// 將欄位字串輸出到 rbuf, 每個欄位前都會加上 chSplitter 字元.
fon9_API void FieldsCellRevPrint(const Fields& fields, const RawRd& rd, RevBuffer& rbuf, char chSplitter = GridViewResult::kCellSplitter);
/// \ingroup seed
/// 將欄位字串輸出到 rbuf, 除了第0個欄位, 其餘每個欄位前都會加上 chSplitter 字元.
fon9_API void FieldsCellRevPrint0NoSpl(const Fields& fields, const RawRd& rd, RevBuffer& rbuf, char chSplitter = GridViewResult::kCellSplitter);

template <class Iterator>
void SimpleMakeRowView(Iterator ivalue, Tab* tab, RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*ivalue}, rbuf, GridViewResult::kCellSplitter);
   RevPrint(rbuf, ivalue->first);
}

/// \ingroup seed
/// 將欄位名稱輸出到 rbuf, 每個欄位前都會加上 chSplitter 字元.
/// 如果 layout != nullptr 則會輸出 layout->KeyField_->Name_;
fon9_API void FieldsNameRevPrint(const Layout* layout, const Tab& tab, RevBuffer& rbuf, char chSplitter = GridViewResult::kCellSplitter);

/// \ingroup seed
/// 將 container 的內容, 使用 SimpleMakeRowView() 輸出到 rbuf;
/// - 每筆資料之間使用 GridViewResult::kRowSplitter 分隔
/// - 尾端沒有分隔, 頭端固定有一個分隔.
template <class Container>
void SimpleMakeFullGridView(const Container& container, Tab& tab, RevBuffer& rbuf) {
   typename Container::const_iterator iend = container.end();
   typename Container::const_iterator ibeg = container.begin();
   while (ibeg != iend) {
      SimpleMakeRowView(--iend, &tab, rbuf);
      RevPrint(rbuf, GridViewResult::kRowSplitter);
   }
}


template <class Iterator>
inline auto IteratorForwardDistance(Iterator icur, Iterator ifrom) -> decltype(static_cast<size_t>(icur - ifrom)) {
   return static_cast<size_t>(icur - ifrom);
}
inline size_t IteratorForwardDistance(...) {
   return static_cast<size_t>(GridViewResult::kNotSupported);
}

/// \ingroup seed
/// 協助 TreeOp::GridView().
/// fnRowAppender 可參考 SimpleMakeRowView();
/// 最後一列不含 kRowSplitter。
template <class Iterator, class FnRowAppender>
void MakeGridViewRange(Iterator istart, Iterator ibeg, Iterator iend,
                       const GridViewRequest& req, GridViewResult& res,
                       FnRowAppender fnRowAppender) {
   auto offset = req.Offset_;
   if (offset < 0) {
      while (istart != ibeg) {
         --istart;
         if (++offset >= 0)
            break;
      }
   }
   else if (offset > 0) {
      while (istart != iend) {
         ++istart;
         if (--offset <= 0)
            break;
      }
   }
   res.DistanceBegin_ = (istart == ibeg) ? 0u : IteratorForwardDistance(istart, ibeg);
   if (istart == iend)
      res.DistanceEnd_ = 0;
   else {
      RevBufferList  rbuf{256};
      size_t         lastLinePos = 0;
      for (;;) {
         fnRowAppender(istart, req.Tab_, rbuf);
         if (!res.GridView_.empty())
            res.GridView_.push_back(res.kRowSplitter);
         lastLinePos = res.GridView_.size();
         BufferAppendTo(rbuf.MoveOut(), res.GridView_);
         ++res.RowCount_;
         if (req.MaxRowCount_ > 0 && res.RowCount_ >= req.MaxRowCount_)
            break;
         if (req.MaxBufferSize_ > 0 && (res.GridView_.size() >= req.MaxBufferSize_))
            break;
         if (++istart == iend)
            break;
      }
      res.SetLastKey(lastLinePos);
      res.DistanceEnd_ = (istart == iend) ? 1u : IteratorForwardDistance(iend, istart);
   }
}

/// \ingroup seed
/// 協助 TreeOp::GridView().
/// fnRowAppender 可參考 SimpleMakeRowView();
template <class Container, class Iterator, class FnRowAppender>
void MakeGridView(Container& container, Iterator istart,
                  const GridViewRequest& req, GridViewResult& res,
                  FnRowAppender&& fnRowAppender) {
   res.SetContainerSize(&container);
   MakeGridViewRange(istart, container.begin(), container.end(),
                     req, res, std::forward<FnRowAppender>(fnRowAppender));
}

/// \ingroup seed
/// 若使用 unordered_map 則 req.OrigKey_ = "key list";
template <class Container, class Iterator, class FnMakeRowView>
void MakeGridViewUnordered(const Container& container, Iterator iend,
                           const GridViewRequest& req, GridViewResult& res,
                           FnMakeRowView fnMakeRowView) {
   if (req.OrigKey_.Get1st() != GridViewResult::kCellSplitter)
      res.OpResult_ = OpResult::not_supported_grid_view;
   else {
      res.SetContainerSize(&container);
      StrView        keys{req.OrigKey_.begin() + 1, req.OrigKey_.end()};
      RevBufferList  rbuf{256};
      for (;;) {
         const char* pkey = static_cast<const char*>(memrchr(keys.begin(), GridViewResult::kCellSplitter, keys.size()));
         StrView     key{pkey ? (pkey + 1) : keys.begin(), keys.end()};
         auto        ifind = GetIteratorForPod(container, key);
         if ((ifind == iend) || !fnMakeRowView(ifind, res.Tab_, rbuf))
            RevPrint(rbuf, key);
         if (pkey == nullptr)
            break;
         keys.SetEnd(pkey);
         RevPrint(rbuf, GridViewResult::kRowSplitter);
      }
      res.GridView_ = BufferTo<std::string>(rbuf.MoveOut());
   }
}

/// \ingroup seed
/// 協助 TreeOp::GridView().
/// fnRowAppender 可參考 SimpleMakeRowView(); 但須傳回 true 表示有資料, false 表示無資料.
/// 最後一列不含 kRowSplitter。
template <class Iterator, class FnRowAppender>
void MakeGridViewArrayRange(Iterator istart, Iterator iend,
                            const GridViewRequest& req, GridViewResult& res,
                            FnRowAppender fnRowAppender) {
   if (req.Offset_ < 0) {
      if (istart >= static_cast<Iterator>(-req.Offset_))
         istart += req.Offset_;
      else
         istart = Iterator{};
   }
   else if (req.Offset_ > 0) {
      if (unsigned_cast(iend - istart) <= unsigned_cast(req.Offset_))
         istart = iend;
      else
         istart += req.Offset_;
   }
   res.DistanceBegin_ = unsigned_cast(istart - Iterator{});
   if (istart < iend) {
      RevBufferList  rbuf{256};
      size_t         lastLinePos = 0;
      for (;;) {
         if (!fnRowAppender(istart, req.Tab_, rbuf)) {
            assert(rbuf.cfront() == nullptr);
            if (++istart == iend)
               break;
            continue;
         }
         if (!res.GridView_.empty())
            res.GridView_.push_back(res.kRowSplitter);
         lastLinePos = res.GridView_.size();
         BufferAppendTo(rbuf.MoveOut(), res.GridView_);
         ++res.RowCount_;
         if (req.MaxRowCount_ > 0 && res.RowCount_ >= req.MaxRowCount_)
            break;
         if (req.MaxBufferSize_ > 0 && (res.GridView_.size() >= req.MaxBufferSize_))
            break;
         if (++istart == iend)
            break;
      }
      res.SetLastKey(lastLinePos);
      res.DistanceEnd_ = (istart == iend) ? 1u : unsigned_cast(iend - istart);
   }
   else {
      res.DistanceEnd_ = 0u;
   }
}

//--------------------------------------------------------------------------//

template <class TreeOp, class MustLockContainer, class FnRowAppender>
void TreeOp_GridView_MustLock(TreeOp& op, MustLockContainer& c,
                              const GridViewRequest& req, FnGridViewOp&& fnCallback,
                              FnRowAppender&& fnRowAppender) {
   GridViewResult res{op.Tree_, req.Tab_};
   {
      typename MustLockContainer::Locker container{c};
      MakeGridView(*container, GetIteratorForGv(*container, req.OrigKey_),
                   req, res, std::move(fnRowAppender));
   } // unlock.
   fnCallback(res);
}

template <class PodOp, class TreeOp, class MustLockContainer>
void TreeOp_Get_MustLock(TreeOp& treeOp, MustLockContainer& c, StrView strKeyText, FnPodOp&& fnCallback) {
   {
      typename MustLockContainer::Locker container{c};
      auto   ifind = GetIteratorForPod(*container, strKeyText);
      if (ifind != container->end()) {
         PodOp podOp{*ifind, treeOp.Tree_, OpResult::no_error, strKeyText, container};
         fnCallback(podOp, &podOp);
         return;
      }
   } // unlock.
   fnCallback(PodOpResult{treeOp.Tree_, OpResult::not_found_key, strKeyText}, nullptr);
}

} } // namespaces
#endif//__fon9_seed_TreeOp_hpp__
