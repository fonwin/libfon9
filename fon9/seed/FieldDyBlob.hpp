/// \file fon9/seed/FieldDyBlob.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldDyBlob_hpp__
#define __fon9_seed_FieldDyBlob_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/SortedVector.hpp"
#include "fon9/Blob.h"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 存取 DyBlob 的欄位.
/// 不提供 fon9_Blob 的 data member 欄位, 應透過 FieldByteVector 使用 ByteVector(間接使用 fon9_Blob).
class fon9_API FieldDyBlob : public Field {
   fon9_NON_COPY_NON_MOVE(FieldDyBlob);
   typedef Field  base;

protected:
   // do nothing.
   virtual void OnCellChanged(const RawWr&) const;
   OpResult StrToCellNoEvent(const RawWr& wr, StrView value) const;

public:
   /// 建構: 固定為 FieldSource::DyBlob;
   FieldDyBlob(Named&& named, f9sv_FieldType type)
      : base{std::move(named), type, FieldSource::DyBlob, 0, sizeof(fon9_Blob), 0} {
   }

   /// 取得 bytes 內容, 若 this->Type_ == f9sv_FieldType_Chars 則可視為字串使用, 否則應視為 byte[];
   StrView GetValue(const RawRd& rd) const {
      fon9_Blob blob = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
      return StrView{reinterpret_cast<char*>(blob.MemPtr_), blob.MemUsed_};
   }

   /// 傳回: this->Type_ == f9sv_FieldType_Chars ? "C0 : "B0";
   virtual StrView GetTypeId(NumOutBuf&) const override;

   /// 若 this->Type_ == f9sv_FieldType_Chars 內容直接字串輸出.
   /// 否則輸出 base64 字串, 此時不理會 fmt.
   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;

   /// 若 this->Type_ == f9sv_FieldType_Chars 直接將 value 填入.
   /// 否則將 value 視為 base64 字串轉成 bytes 之後填入.
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override;

   virtual void CellToBitv(const RawRd& rd, RevBuffer& out) const override;
   virtual OpResult BitvToCell(const RawWr& wr, DcQueue& buf) const override;

   /// 清除資料: 將 blob 資料大小設為空.
   virtual OpResult SetNull(const RawWr& wr) const override;
   /// \return blob.empty()
   virtual bool IsNull(const RawRd& rd) const override;

   /// 不支援, 傳回 nullValue;
   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override;
   /// 不支援, 不改變 wr, 傳回 OpResult::not_supported_number.
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override;

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override;
   virtual int Compare(const RawRd& lhs, const RawRd& rhs) const override;
   virtual size_t AppendRawBytes(const RawRd& rd, ByteVector& dst) const override;
   virtual int CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const override;
};

//--------------------------------------------------------------------------//

/// \ingroup seed
/// 字串列舉欄位: CellRevPrint(); 使用 Description 的列舉設定;
/// 列舉設定方式: {Tag1=Value1|Tag2=Value2|...} 大括號可省略;
class fon9_API FieldStrEnum : public FieldDyBlob {
   fon9_NON_COPY_NON_MOVE(FieldStrEnum);
   using base = FieldDyBlob;
   struct EnumList {
      std::string OrigEnumStr_; // 檢查 Description 如果有異動, 則要重新建立列舉對照.
      SortedVector<StrView, StrView> IdEnumMap_;

      void RebuildEnumList(StrView cfgs);

      StrView GetEnumValue(StrView id) const {
         auto ifind = this->IdEnumMap_.find(id);
         return ifind == this->IdEnumMap_.end() ? id : ifind->second;
      }
   };
   EnumList EnumList_;

public:
   /// 在建構時設定 EnumList; 可避免 multi thread 同時取用 EnumList 的問題;
   FieldStrEnum(Named&& named) : base{std::move(named), f9sv_FieldType_Chars} {
      this->EnumList_.RebuildEnumList(&this->Description_);
   }

   void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;
};

} } // namespaces
#endif//__fon9_seed_FieldDyBlob_hpp__
