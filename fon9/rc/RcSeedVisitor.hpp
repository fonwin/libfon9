// \file fon9/rc/RcSeedVisitor.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitor_hpp__
#define __fon9_rc_RcSeedVisitor_hpp__
#include "fon9/rc/RcSeedVisitor.h"
#include "fon9/seed/SeedSubr.hpp"
#include "fon9/Utility.hpp"
#include "fon9/NamedIx.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/buffer/DcQueue.hpp"
#include <limits> // std::numeric_limits<f9sv_TabSize>::max()

namespace fon9 { namespace rc {

enum class SvFuncFlag : uint8_t {
   Layout = 0x10,
};
enum class SvFuncCode : uint8_t {
   Empty = 0x00,
   /// 登入成功後的「可用路徑」及權限.
   /// 透過 fon9::rc::SvParseGvTables() 解析.
   Acl = 1,
   /// 查詢 Seed or Pod.
   /// - 可選擇是否要查詢 layout:
   ///   - 同一個 tree 的 layout 只需要查詢一次, client 應將 layout 保留.
   Query = 2,
   /// 訂閱.
   Subscribe = 3,
   /// 取消訂閱.
   Unsubscribe = 4,
   /// 訂閱資料回覆 Server to Client.
   SubscribeData = 5,
};

/// SvFunc 功能碼.
/// - RcSeedVisitorServerNote::OnRecvFunctionCall(); RcSeedVisitorClientNote::OnRecvFunctionCall();
///   收到的 first byte.
/// - 分成: 0xf0 旗標 SvFuncFlag, 及 0x0f 功能 SvFuncCode.
enum class SvFunc : uint8_t {
   Empty = 0x00,
   FuncMask = 0x0f,
};
constexpr bool HasSvFuncFlag(SvFunc src, SvFuncFlag flag) {
   return (cast_to_underlying(src) & cast_to_underlying(flag)) != 0;
}
constexpr SvFunc ToSvFunc(SvFuncCode src) {
   return static_cast<SvFunc>(cast_to_underlying(src));
}
constexpr SvFuncCode GetSvFuncCode(SvFunc src) {
   return static_cast<SvFuncCode>(cast_to_underlying(src) & cast_to_underlying(SvFunc::FuncMask));
}
inline SvFunc SetSvFuncCode(SvFunc* src, SvFuncCode fnCode) {
   assert((cast_to_underlying(fnCode) & ~cast_to_underlying(SvFunc::FuncMask)) == 0);
   return *src = static_cast<SvFunc>(cast_to_underlying(fnCode)
      | (cast_to_underlying(*src) & ~cast_to_underlying(SvFunc::FuncMask)));
}
inline SvFunc SetSvFuncFlag(SvFunc* src, SvFuncFlag flag) {
   assert((cast_to_underlying(flag) & cast_to_underlying(SvFunc::FuncMask)) == 0);
   return *src = static_cast<SvFunc>(cast_to_underlying(*src) | cast_to_underlying(flag));
}
inline SvFunc ClrSvFuncFlag(SvFunc* src, SvFuncFlag flag) {
   assert((cast_to_underlying(flag) & cast_to_underlying(SvFunc::FuncMask)) == 0);
   return *src = static_cast<SvFunc>(cast_to_underlying(*src) & ~cast_to_underlying(flag));
}
inline SvFunc SvFuncSubscribeData(seed::SeedNotifyKind v) {
   return static_cast<SvFunc>(cast_to_underlying(SvFuncCode::SubscribeData)
                              | (cast_to_underlying(v) << 4));
}
inline seed::SeedNotifyKind GetSvFuncSubscribeDataNotifyKind(SvFunc src) {
   assert(GetSvFuncCode(src) == SvFuncCode::SubscribeData);
   return static_cast<seed::SeedNotifyKind>(cast_to_underlying(src) >> 4);
}

//--------------------------------------------------------------------------//
/// T 必須包含 f9sv_Named T::Named_;
template <class T>
struct NamedWrap : public T {
   NamedWrap() {
      ZeroStruct(static_cast<T*>(this));
      this->Named_.Index_ = -1;
   }
   NamedWrap(const StrView& name) : NamedWrap{} {
      this->Named_.Name_ = name.ToCStrView();
   }

   StrView GetNameStr() const { return this->Named_.Name_; }
   int     GetIndex()   const { return this->Named_.Index_; }
   bool SetIndex(size_t index) {
      if (this->Named_.Index_ >= 0)
         return false;
      this->Named_.Index_ = static_cast<int>(index);
      return true;
   }
};
//--------------------------------------------------------------------------//
/// C API: f9sv_Field; 對應的 C++ 型別.
class SvField : public NamedWrap<f9sv_Field> {
   using base = NamedWrap<f9sv_Field>;
public:
   using base::base;
   SvField() = default;

   // 為了提供給 NamedIxMapNoRemove<> 使用, 所以提供底下 members.
   using pointer = const SvField*;
   using element_type = SvField;
   bool           operator!()  const { return false; }
   SvField*       operator->() { return this; }
   const SvField* operator->() const { return this; }
   const SvField* get()        const { return this; }
};
using SvFields = NamedIxMapNoRemove<SvField>;

//--------------------------------------------------------------------------//

/// C API: f9sv_GvTable; 對應的 C++ 型別.
class fon9_API SvGvTable : public NamedWrap<f9sv_GvTable> {
   fon9_NON_COPY_NON_MOVE(SvGvTable);
   using base = NamedWrap<f9sv_GvTable>;
public:
   using RecList = std::vector<const StrView*>;
   f9sv_GvList GvList_;
   SvFields    Fields_;
   RecList     RecList_;

   using base::base;
   SvGvTable() = default;
   ~SvGvTable();

   void InitializeGvList();
};
using SvGvTableSP = std::unique_ptr<SvGvTable>;
static_assert(sizeof(const SvGvTableSP) == sizeof(const f9sv_GvTable*),
              "SvGvTables::NamedIxSP cannot be a plain pointer.");
using SvGvTables = NamedIxMapNoRemove<SvGvTableSP>;

/// - 請參考 fon9_kCSTR_LEAD_TABLE 提供的格式說明.
/// - srcstr 會被改變('\n' or '\x01' 變成 '\0'),
/// - 在 dst 的有效期間, srcstr 必須有效.
/// - 解析前 **不會** 先清除 dst.
fon9_API void SvParseGvTables(SvGvTables& dst, std::string& srcstr);

/// - dst.clear();
/// - 複製 orig 到 strbuf, 然後透過 SvParseGvTables() 解析.
/// - strbuf 提供給 SvParseGvTables() 解析使用的訊息, 分隔符號變成 EOS.
/// - 設定 cTables;
fon9_API void SvParseGvTablesC(f9sv_GvTables& cTables, SvGvTables& dst, std::string& strbuf, StrView orig);

//--------------------------------------------------------------------------//

constexpr f9sv_TabSize     kTabAll = std::numeric_limits<f9sv_TabSize>::max();
constexpr f9sv_SubrIndex   kSubrIndexNull = std::numeric_limits<f9sv_SubrIndex>::max();

struct fon9_API RcSvReqKey {
   CharVector     TreePath_, SeedKey_, TabName_;
   /// 當 IsAllTabs_ 時, 不應理會 TabName_ 的內容.
   /// 通常用於查詢 pod;
   bool           IsAllTabs_;
   char           Padding___[3];
   f9sv_SubrIndex SubrIndex_{kSubrIndexNull};

   /// 從 rxbuf 載入 TreePath_, SeedKey_, TabName_; 不包含 SubrIndex_;
   /// - 如果 rxbuf 提供的 tab 為 index, 則會自動填入:
   ///   - TabName_ = "n"; n = tab index;
   ///   - TabName_ = "*"; IsAllTabs_ = true;
   void LoadSeedName(DcQueue& rxbuf);

   /// 除了 LoadSeedName() 之外,
   /// 若 GetSvFuncCode(fc) == SvFuncCode::Subscribe; 則在 SeedName 之後包含 this->SubrIndex_;
   void LoadFrom(SvFunc fc, DcQueue& rxbuf);
};
/// 輸出: "|path=", reqKey.TreePath_, "|key=", reqKey.SeedKey_, "|tab=" ...
fon9_API void RevPrint(RevBuffer& rbuf, const RcSvReqKey& reqKey);

} } // namespaces
#endif//__fon9_rc_RcSeedVisitor_hpp__
