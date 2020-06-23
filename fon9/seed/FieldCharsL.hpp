/// \file fon9/seed/FieldCharsL.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldCharsL_hpp__
#define __fon9_seed_FieldCharsL_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/CharAryL.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 存取 CharAryL<> 的欄位.
/// 長度限制只能 1..255, 因為超過 255, 長度欄位超過 1 byte, 可能會有記憶體對齊問題.
class fon9_API FieldCharsL : public Field {
   fon9_NON_COPY_NON_MOVE(FieldCharsL);
   using base = Field;
public:
   /// 建構:
   /// - 固定為 f9sv_FieldType_Chars; FieldSource::DataMember;
   /// - size 必須 1..255, 若有動態調整大小的需求, 應使用 FieldStdString 或 FieldCharVector.
   /// - this->Size_ = size + 1; 因為需要一個 byte 儲存字串長度.
   FieldCharsL(Named&& named, int32_t ofs, size_t size)
      : base(std::move(named), f9sv_FieldType_Chars, FieldSource::DataMember, ofs, size+1, 0) {
      assert(1 <= size && size <= 255);
   }
   /// DyMem 建構, 所以不用提供 ofs:
   /// - 固定為 f9sv_FieldType_Chars; FieldSource::DyMem;
   /// - this->Size_ = size + 1; 因為需要一個 byte 儲存字串長度.
   FieldCharsL(Named&& named, size_t size)
      : base(std::move(named), f9sv_FieldType_Chars, FieldSource::DyMem, 0, size+1, 0) {
      assert(1 <= size && size <= 255);
   }

   uint32_t MaxSize() const {
      return this->Size_ - 1;
   }
   StrView GetValue(const RawRd& rd) const {
      const char* ptr = rd.GetCellPtr<char>(*this);
      return StrView{ptr+1, static_cast<uint8_t>(*ptr)};
   }
   StrView GetStrView(const RawRd& rd) const {
      return this->GetValue(rd);
   }

   /// 傳回: "CnL"; n = this->Size_ - 1;
   virtual StrView GetTypeId(NumOutBuf&) const override;

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override;
   virtual void CellToBitv(const RawRd& rd, RevBuffer& out) const override;
   virtual OpResult BitvToCell(const RawWr& wr, DcQueue& buf) const override;

   /// 將長度設為 0, 其餘不變.
   virtual OpResult SetNull(const RawWr& wr) const override;
   /// \retval 長度 == 0;
   virtual bool IsNull(const RawRd& rd) const override;

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override;
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override;

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override;
   virtual int Compare(const RawRd& lhs, const RawRd& rhs) const override;
};

template <size_t arysz>
inline FieldSPT<FieldCharsL> MakeField(Named&& named, int32_t ofs, CharAryL<arysz, char>&) {
   return FieldSPT<FieldCharsL>{new FieldCharsL{std::move(named), ofs, arysz}};
}
template <size_t arysz>
inline FieldSPT<FieldCharsL> MakeField(Named&& named, int32_t ofs, const CharAryL<arysz, char>&) {
   return FieldSPT<FieldCharsL>{new FieldConst<FieldCharsL>{std::move(named), ofs, arysz}};
}
} } // namespaces
#endif//__fon9_seed_FieldCharsL_hpp__
