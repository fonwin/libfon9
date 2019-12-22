// \file fon9/rc/RcSeedVisitor.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitor_hpp__
#define __fon9_rc_RcSeedVisitor_hpp__
#include "fon9/rc/RcSeedVisitor.h"
#include "fon9/Utility.hpp"
#include "fon9/NamedIx.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/buffer/DcQueue.hpp"
#include <limits> // std::numeric_limits<f9sv_TabSize>::max()

namespace fon9 { namespace rc {

enum class SvFuncCode : uint8_t {
   FuncMask = 0xf0,
   /// 登入成功後的「可用路徑」及權限.
   /// 透過 fon9::rc::SvParseGvTables() 解析.
   Acl = 0x00,
   /// 查詢 Seed or Pod.
   /// - 可選擇是否要查詢 layout:
   ///   - 同一個 tree 的 layout 只需要查詢一次, client 應將 layout 保留.
   Query = 0x10,
   /// 訂閱.
   Subscribe = 0x20,
   /// 取消訂閱.
   Unsubscribe = 0x40,

   FlagLayout = 0x01,
};
fon9_ENABLE_ENUM_BITWISE_OP(SvFuncCode);

//--------------------------------------------------------------------------//

/// C API: f9sv_Named; 對應的 C++ 型別.
struct SvNamed : public f9sv_Named {
   SvNamed() {
      memset(this, 0, sizeof(*this));
      this->Index_ = -1;
   }
   SvNamed(StrView name) : SvNamed{} {
      this->Name_ = name.ToCStrView();
   }

   StrView GetNameStr() const { return this->Name_; }
   int     GetIndex()   const { return this->Index_; }
   bool SetIndex(size_t index) {
      if (this->Index_ >= 0)
         return false;
      this->Index_ = static_cast<int>(index);
      return true;
   }
};

fon9_GCC_WARN_DISABLE("-Winvalid-offsetof");
/// C API: f9sv_Field; 對應的 C++ 型別.
struct SvField : public SvNamed {
   char        TypeId_[sizeof(f9sv_Field::TypeId_)];
   const void* InternalOwner_{};

   void CtorInit() {
      memset(this->TypeId_, 0, sizeof(this->TypeId_));
   }

   SvField() {
      this->CtorInit();
   }
   SvField(StrView name) : SvNamed{name} {
      this->CtorInit();
   }
   f9sv_Field* ToPlainC() {
      static_assert(offsetof(SvField, Name_) == offsetof(f9sv_Field, Named_.Name_)
                    && offsetof(SvField, TypeId_) == offsetof(f9sv_Field, TypeId_)
                    && sizeof(SvField) == sizeof(f9sv_Field),
                    "SvField must be the same as f9sv_Field.");
      return reinterpret_cast<f9sv_Field*>(this);
   }
   const f9sv_Field* ToPlainC() const {
      return reinterpret_cast<const f9sv_Field*>(this);
   }

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
struct fon9_API SvGvTable : public SvNamed {
   fon9_NON_COPY_NON_MOVE(SvGvTable);
   using RecList = std::vector<const StrView*>;
   f9sv_GvList GvList_;
   SvFields    Fields_;
   RecList     RecList_;

   using SvNamed::SvNamed;
   SvGvTable() = default;
   ~SvGvTable();

   void InitializeGvList();
};
static_assert(offsetof(SvGvTable, Name_) == offsetof(f9sv_GvTable, Named_.Name_)
              && offsetof(SvGvTable, GvList_) == offsetof(f9sv_GvTable, GvList_),
              "SvGvTable must be the same as f9sv_GvTable.");
fon9_GCC_WARN_POP;

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

struct fon9_API RcSvReqKey {
   CharVector  TreePath_, SeedKey_, TabName_;
   bool        IsAllTabs_;
   char        Padding___[7];

   void LoadFrom(DcQueue& rxbuf);
};
/// 輸出: "path=", reqKey.TreePath_, "|key=", reqKey.SeedKey_, "|tab=" ...
fon9_API void RevPrint(RevBuffer& rbuf, const RcSvReqKey& reqKey);

constexpr f9sv_TabSize  kTabAll = std::numeric_limits<f9sv_TabSize>::max();
constexpr bool IsTabAll(const char* tabName) {
   return (tabName[0] == '*' && tabName[1] == '\0');
}

} } // namespaces
#endif//__fon9_rc_RcSeedVisitor_hpp__
