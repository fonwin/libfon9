// \file fon9/fmkt/SymbTree.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTree_hpp__
#define __fon9_fmkt_SymbTree_hpp__
#include "fon9/fmkt/Symb.hpp"
#include "fon9/seed/Tree.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace fmkt {

class fon9_API SymbPodOp : public seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(SymbPodOp);
   using base = seed::PodOpDefault;

   virtual void OnAfterPodOpWrite(seed::Tab& tab);
public:
   const SymbSP   Symb_;
   SymbPodOp(seed::Tree& sender, const StrView& keyText, SymbSP&& symb)
      : base(sender, seed::OpResult::no_error, keyText)
      , Symb_{std::move(symb)} {
   }

   void BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) override;
   void BeginWrite(seed::Tab& tab, seed::FnWriteOp fnCallback) override;
};

class SymbTreeOp : public seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(SymbTreeOp);
   using base = seed::TreeOp;
public:
   using base::base;

   template <class Iterator>
   static bool MakeRowView(Iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
      if (tab) {
         if (auto dat = GetSymbValue(*ivalue).GetSymbData(tab->GetIndex()))
            FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{*dat}, rbuf, seed::GridViewResult::kCellSplitter);
      }
      RevPrint(rbuf, GetSymbKey(*ivalue));
      return true;
   }
   /// 如果不是 unordered_map, 則可以使用一般的 GridView.
   template <class Container>
   static void GridViewOrdered(const Container& c, const seed::GridViewRequest& req, seed::GridViewResult& res) {
      seed::MakeGridView(c, seed::GetIteratorForGv(c, req.OrigKey_), req, res, &MakeRowView<typename Container::const_iterator>);
   }
   /// 若使用 unordered_map 則 req.OrigKey_ = "key list";
   template <class Container>
   static void GridViewUnordered(const Container& c, const seed::GridViewRequest& req, seed::GridViewResult& res) {
      seed::MakeGridViewUnordered(c, c.end(), req, res, &MakeRowView<typename Container::const_iterator>);
   }

   template <class Container>
   static void GetHasher(...);

   template <class Container>
   static auto GetHasher(int) -> decltype(static_cast<Container*>(nullptr)->hash_function());

   template <class Container>
   struct TestHasHasher {
      enum { HasHasher = !std::is_same<decltype(GetHasher<Container>(1)), void>::value };
   };

   template <class Container>
   static enable_if_t<TestHasHasher<Container>::HasHasher>
      MakeGridView(const Container& c, const seed::GridViewRequest& req, seed::GridViewResult& res) {
      GridViewUnordered(c, req, res);
   }

   template <class Container>
   static enable_if_t<!TestHasHasher<Container>::HasHasher>
      MakeGridView(const Container& c, const seed::GridViewRequest& req, seed::GridViewResult& res) {
      GridViewOrdered(c, req, res);
   }
};
//--------------------------------------------------------------------------//
template <class SymbMapImplT, class MutexT>
class SymbTreeT : public seed::Tree {
   fon9_NON_COPY_NON_MOVE(SymbTreeT);
   using base = seed::Tree;

public:
   using SymbMapImpl = SymbMapImplT;
   using iterator = typename SymbMapImpl::iterator;
   using SymbMap = MustLock<SymbMapImpl, MutexT>;
   using Locker = typename SymbMap::Locker;
   SymbMap  SymbMap_;

   using base::base;
   void OnTreeOp(seed::FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
   }
   void OnParentSeedClear() override {
      SymbMapImpl symbs{std::move(*this->SymbMap_.Lock())};
      // unlock 後, symbs 解構時, 自動清除.
   }

   /// PodOp 操作資料異動後通知.
   /// - 讓 SymbTreeT 的衍生者(實際應用者), 可以建立商品必要的關聯.
   /// - 預設: do nothing.
   virtual void OnAfterPodOpWrite(Symb& symb, seed::Tab& tab, const Locker& symbs) {
      (void)symb; (void)tab; (void)symbs;
   }

   /// 衍生者必須傳回有效的 SymbSP;
   virtual SymbSP MakeSymb(const StrView& symbid) = 0;

   SymbSP FetchSymb(const Locker& symbs, const StrView& symbid) {
      auto ifind = symbs->find(symbid);
      if (ifind != symbs->end())
         return SymbSP{&GetSymbValue(*ifind)};
      return InsertSymb(*symbs, this->MakeSymb(symbid));
   }
   SymbSP FetchSymb(const StrView& symbid) {
      return this->FetchSymb(this->SymbMap_.Lock(), symbid);
   }

   static SymbSP GetSymb(const Locker& symbs, const StrView& symbid) {
      auto ifind = symbs->find(symbid);
      return ifind == symbs->end() ? SymbSP{nullptr} : SymbSP{&GetSymbValue(*ifind)};
   }
   SymbSP GetSymb(const StrView& symbid) {
      return this->GetSymb(this->SymbMap_.Lock(), symbid);
   }

protected:
   class PodOp : public SymbPodOp {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = SymbPodOp;
   public:
      const Locker&  LockedMap_;
      PodOp(Tree& sender, const StrView& keyText, SymbSP&& symb, const Locker& lockedMap)
         : base(sender, keyText, std::move(symb))
         , LockedMap_(lockedMap) {
      }
      void OnAfterPodOpWrite(seed::Tab& tab) override {
         static_cast<SymbTreeT*>(this->Sender_)->OnAfterPodOpWrite(*this->Symb_, tab, this->LockedMap_);
      }
   };
   class TreeOp : public SymbTreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = SymbTreeOp;
   public:
      TreeOp(SymbTreeT& tree) : base{tree} {
      }

      void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) override {
         seed::GridViewResult res{this->Tree_, req.Tab_};
         {
            Locker lockedMap{static_cast<SymbTreeT*>(&this->Tree_)->SymbMap_};
            this->MakeGridView(*lockedMap, req, res);
         } // unlock map.
         fnCallback(res);
      }

      virtual void OnSymbPodOp(const StrView& strKeyText, SymbSP&& symb, seed::FnPodOp&& fnCallback, const Locker& lockedMap) {
         PodOp op(this->Tree_, strKeyText, std::move(symb), lockedMap);
         fnCallback(op, &op);
      }
      void OnPodOp(const StrView& strKeyText, SymbSP&& symb, seed::FnPodOp&& fnCallback, const Locker& lockedMap) {
         if (symb)
            this->OnSymbPodOp(strKeyText, std::move(symb), std::move(fnCallback), lockedMap);
         else
            fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
      }
      void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
         Locker lockedMap{static_cast<SymbTreeT*>(&this->Tree_)->SymbMap_};
         SymbSP symb = static_cast<SymbTreeT*>(&this->Tree_)->GetSymb(lockedMap, strKeyText);
         this->OnPodOp(strKeyText, std::move(symb), std::move(fnCallback), lockedMap);
      }
      void Add(StrView strKeyText, seed::FnPodOp fnCallback) override {
         if (seed::IsTextBeginOrEnd(strKeyText))
            fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
         else {
            Locker lockedMap{static_cast<SymbTreeT*>(&this->Tree_)->SymbMap_};
            SymbSP symb = static_cast<SymbTreeT*>(&this->Tree_)->FetchSymb(lockedMap, strKeyText);
            this->OnPodOp(strKeyText, std::move(symb), std::move(fnCallback), lockedMap);
         }
      }
      void Remove(StrView strKeyText, seed::Tab* tab, seed::FnPodRemoved fnCallback) override {
         seed::PodRemoveResult res{this->Tree_, seed::OpResult::not_found_key, strKeyText, tab};
         {
            Locker lockedMap{static_cast<SymbTreeT*>(&this->Tree_)->SymbMap_};
            auto   ifind = lockedMap->find(strKeyText);
            if (ifind != lockedMap->end()) {
               lockedMap->erase(ifind);
               res.OpResult_ = seed::OpResult::removed_pod;
            }
         }
         fnCallback(res);
      }
   };
};
//--------------------------------------------------------------------------//

/// \ingroup fmkt
/// 商品資料表, 一般行情系統使用: multi thread(mutex) + unordered
class fon9_API SymbTree : public SymbTreeT<SymbMap, std::mutex> {
   fon9_NON_COPY_NON_MOVE(SymbTree);
public:
   using SymbTreeT::SymbTreeT;
   ~SymbTree();

   void LockedDailyClear(Locker& symbs, unsigned tdayYYYYMMDD);
};
// 使用 fon9_API_TEMPLATE_CLASS 造成 VS 2015 Debug build 失敗?!
// fon9_API_TEMPLATE_CLASS(SymbTree, SymbTreeT, SymbMap, std::mutex);

} } // namespaces
#endif//__fon9_fmkt_SymbTree_hpp__
