/// \file fon9/seed/SeedBase.hpp
/// \author fonwinz@gmail.com
/// \defgroup seed  管理機制
#ifndef __fon9_seed_SeedBase_hpp__
#define __fon9_seed_SeedBase_hpp__
#include "fon9/StrView.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include <memory> // std::unique_ptr<>

namespace fon9 { namespace seed {

class RawRd;
class RawWr;
class Raw;

class Field;
template <class F>
using FieldSPT = std::unique_ptr<F>;
using FieldSP = FieldSPT<Field>;

class Tab;
using TabSP = intrusive_ptr<Tab>;

class Tree;
template <class T>
using TreeSPT = intrusive_ptr<T>;
using TreeSP  = TreeSPT<Tree>;
class TreeOp;

class Layout;
/// \ingroup seed
/// 同一個 Layout 物件可以給多個 Tree 參考使用.
using LayoutSP = intrusive_ptr<Layout>;

enum class OpResult {
   no_error = 0,

   removed_pod,
   removed_seed,

   mem_alloc_error = -1,
   access_denied = -2,
   path_format_error = -3,

   /// -10 .. -30 保留給 fon9/rc/RcSeedVisitor.h 裡面的 enum f9sv_Result;

   key_exists         = -50,
   key_format_error   = -51,
   value_format_error = -52,
   value_overflow     = -53,
   value_underflow    = -54,

   /// 指令的參數錯誤.
   bad_command_argument = -90,

   not_supported_cmd        = -100,

   not_supported_read       = -110,
   not_supported_write      = -111,
   not_supported_null       = -112,
   not_supported_number     = -113,

   not_supported_get_seed   = -120,
   not_supported_get_pod    = -121,
   not_supported_add_pod    = -122,
   not_supported_remove_pod = -123,

   /// 有些 tree 僅允許移除整個 pod, 不允許移除 pod 裡面的一個 seed.
   /// - 在 TreeOp::RemoveSeed(StrView strKeyText, Tab* tab, FnPodOp fnCallback)
   /// - 若有指定 tab, 且不允許移除單一 seed 時, 就會回覆此結果.
   not_supported_remove_seed = -124,

   not_supported_grid_view   = -130,
   not_supported_tree_op     = -131,
   not_supported_tab_tree_op = -132,
   not_supported_grid_apply  = -133,
   /// 在 TreeOp::GridApplySubmit() 比對修改前的確認資料時又有其他變動了!
   /// (1) TreeOp::GridApplyCheck() 比對編輯中資料.
   /// (2) 使用者確認有變動的地方.
   /// (3) TreeOp::GridApplySubmit() 確認變動.
   /// 如果在 (2)=>(3) 之間, 有其他人又變動了「編輯中資料」, 則在執行(3)時會產生此錯誤!
   bad_apply_submit = -134,

   /// 不支援訂閱「異動事件」: SubscribableOp::Subscribe();
   not_supported_subscribe  = -140,
   /// 不支援訂閱「Stream 事件」: SubscribableOp::SubscribeStream();
   not_supported_subscribe_stream = -141,
   /// 訂閱「Stream 事件」提供的 args 參數有誤:
   /// SubscribableOp::SubscribeStream(... StrView args ...);.
   bad_subscribe_stream_args = -142,
   /// 沒有訂閱, 取消失敗.
   no_subscribe = -143,

   not_found_key      = -200,
   /// 指定的 tabName 找不到, 或 tabIndex 有誤.
   not_found_tab      = -201,
   /// 指定的 tab 正確, 但裡面沒有 seed.
   not_found_seed     = -202,
   not_found_sapling  = -203,
   not_found_field    = -204,
};

/// \ingroup seed
fon9_API const char* GetOpResultMessage(OpResult res);

//--------------------------------------------------------------------------//

/// 當用此當作 OnPodOp(), OnPodOpRange() 的參數時, 則表示 container->begin();
static constexpr const char* const  kStrKeyText_Begin_ = nullptr;
static constexpr StrView TextBegin() {
   return StrView{kStrKeyText_Begin_, static_cast<size_t>(0)};
}
static constexpr bool IsTextBegin(const StrView& k) {
   return k.begin() == kStrKeyText_Begin_;
}

/// 當用此當作 OnPodOp(), OnPodOpRange() 的參數時, 則表示 container->end();
/// 也可用在支援 append 的 Tree, 例: AddPod(TextEnd()); 用來表示 append.
static constexpr const intptr_t  kStrKeyText_End_i = -1;
// 避免 gcc 8: reinterpret_cast from integer to pointer
// static constexpr const char* const  kStrKeyText_End_ = reinterpret_cast<const char*>(-1);

static inline StrView TextEnd() {
   return StrView{reinterpret_cast<const char*>(kStrKeyText_End_i), static_cast<size_t>(0)};
}
static constexpr bool IsTextEnd(const StrView& k) {
   // return k.begin() == kStrKeyText_End_; 這樣寫, VC 不允許: C3249: illegal statement or sub-expression for 'constexpr' function
   return reinterpret_cast<intptr_t>(k.begin()) == kStrKeyText_End_i;
}
static constexpr bool IsTextEnd(const char* pText) {
   return reinterpret_cast<intptr_t>(pText) == kStrKeyText_End_i;
}

static constexpr bool IsTextBeginOrEnd(const StrView& k) {
   return IsTextBegin(k) || IsTextEnd(k);
}

//--------------------------------------------------------------------------//

/// 如果 seedKey == "\t" 則表示訂閱 tree; 否則為訂閱指定的 key.
constexpr bool IsSubrTree(const char* seedKey) {
   return seedKey[0] == '\t' && seedKey[1] == '\0';
}
#define fon9_kCSTR_SubrTree   "\t"
static_assert(IsSubrTree(fon9_kCSTR_SubrTree), "");

/// 如果 tabName == "*" 則為 all tabs.
constexpr bool IsTabAll(const char* tabName) {
   return (tabName && tabName[0] == '*' && tabName[1] == '\0');
}
/// 如果 tabName.Get1st() == '$' 則為 SubscribeStream 的訂閱格式:
/// - "$TabName:StreamDecoderName:Args";
constexpr bool IsSubscribeStream(const StrView& tabName) {
   return (tabName.Get1st() == '$');
}
constexpr bool IsSubscribeStream(const char* tabName) {
   return (tabName && tabName[0] == '$');
}

} } // namespaces
#endif//__fon9_seed_SeedBase_hpp__
