/// \file fon9/seed/FieldCharsL.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldCharsL.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace seed {

StrView FieldCharsL::GetTypeId(NumOutBuf& nbuf) const {
   nbuf.SetEOS();
   char* pbeg = nbuf.end();
   *--pbeg = 'L';
   pbeg = UIntToStrRev(pbeg, this->MaxSize());
   *--pbeg = 'C';
   return StrView{pbeg, nbuf.end()};
}
void FieldCharsL::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   FmtRevPrint(fmt, out, this->GetValue(rd));
}
OpResult FieldCharsL::StrToCell(const RawWr& wr, StrView value) const {
   char*  ptr = wr.GetCellPtr<char>(*this);
   size_t sz = std::min(value.size(), static_cast<size_t>(this->MaxSize()));
   memcpy(ptr + 1, value.begin(), sz);
   *ptr = static_cast<char>(sz);
   return OpResult::no_error;
}
void FieldCharsL::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   ToBitv(out, this->GetValue(rd));
}
OpResult FieldCharsL::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   char*  ptr = wr.GetCellPtr<char>(*this);
   size_t sz = BitvToByteArray(buf, ptr + 1, this->MaxSize());
   *ptr = static_cast<char>(sz);
   return OpResult::no_error;
}
OpResult FieldCharsL::SetNull(const RawWr& wr) const {
   *wr.GetCellPtr<char>(*this) = '\0';
   return OpResult::no_error;
}
bool FieldCharsL::IsNull(const RawRd& rd) const {
   return *rd.GetCellPtr<char>(*this) == '\0';
}
FieldNumberT FieldCharsL::GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const {
   return StrToDec(this->GetValue(rd), outDecScale, nullValue);
}
OpResult FieldCharsL::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   NumOutBuf      nbuf;
   char*          pbeg = DecToStrRev(nbuf.end(), num, decScale);
   const size_t   sz = nbuf.GetLength(pbeg);
   if (sz > this->MaxSize())
      return OpResult::value_overflow;
   char*  ptr = wr.GetCellPtr<char>(*this);
   memcpy(ptr + 1, pbeg, sz);
   *ptr = static_cast<char>(sz);
   return OpResult::no_error;
}
OpResult FieldCharsL::Copy(const RawWr& wr, const RawRd& rd) const {
   memcpy(wr.GetCellPtr<char>(*this), rd.GetCellPtr<char>(*this), this->Size_);
   return OpResult::no_error;
}
int FieldCharsL::Compare(const RawRd& lhs, const RawRd& rhs) const {
   return this->GetValue(lhs).Compare(this->GetValue(rhs));
}
size_t FieldCharsL::AppendRawBytes(const RawRd& rd, ByteVector& dst) const {
   dst.append(rd.GetCellPtr<char>(*this), this->Size_);
   return this->Size_;
}
int FieldCharsL::CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const {
   return fon9_CompareBytes(rd.GetCellPtr<byte>(*this), this->Size_, rhs, rsz);
}

} } // namespaces
