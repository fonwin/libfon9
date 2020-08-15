/// \file fon9/seed/FieldChars.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldChars.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace seed {

StrView FieldChars::GetTypeId(NumOutBuf& nbuf) const {
   nbuf.SetEOS();
   char* pbeg = UIntToStrRev(nbuf.end(), this->Size_);
   *--pbeg = 'C';
   return StrView{pbeg, nbuf.end()};
}

void FieldChars::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   FmtRevPrint(fmt, out, this->GetValue(rd));
}

OpResult FieldChars::StrToCell(const RawWr& wr, StrView value) const {
   char* ptr = wr.GetCellPtr<char>(*this);
   size_t sz = std::min(value.size(), static_cast<size_t>(this->Size_));
   memcpy(ptr, value.begin(), sz);
   if (sz < this->Size_)
      ptr[sz] = '\0';
   return OpResult::no_error;
}

void FieldChars::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   ToBitv(out, this->GetValue(rd));
}

OpResult FieldChars::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   BitvToByteArray(buf, wr.GetCellPtr<char>(*this), this->Size_);
   return OpResult::no_error;
}

OpResult FieldChars::SetNull(const RawWr& wr) const {
   assert(this->Size_ > 0);
   *wr.GetCellPtr<char>(*this) = '\0';
   return OpResult::no_error;
}

bool FieldChars::IsNull(const RawRd& rd) const {
   return *rd.GetCellPtr<char>(*this) == '\0';
}

FieldNumberT FieldChars::GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const {
   return StrToDec(this->GetValue(rd), outDecScale, nullValue);
}

OpResult FieldChars::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   NumOutBuf      nbuf;
   char*          pbeg = DecToStrRev(nbuf.end(), num, decScale);
   const size_t   sz = nbuf.GetLength(pbeg);
   if (sz > this->Size_)
      return OpResult::value_overflow;
   char* ptr = wr.GetCellPtr<char>(*this);
   memcpy(ptr, pbeg, sz);
   if (sz < this->Size_)
      ptr[sz] = 0;
   return OpResult::no_error;
}

OpResult FieldChars::Copy(const RawWr& wr, const RawRd& rd) const {
   memcpy(wr.GetCellPtr<char>(*this), rd.GetCellPtr<char>(*this), this->Size_);
   return OpResult::no_error;
}
int FieldChars::Compare(const RawRd& lhs, const RawRd& rhs) const {
   return memcmp(lhs.GetCellPtr<char>(*this), rhs.GetCellPtr<char>(*this), this->Size_);
}
size_t FieldChars::AppendRawBytes(const RawRd& rd, ByteVector& dst) const {
   dst.append(rd.GetCellPtr<char>(*this), this->Size_);
   return this->Size_;
}
int FieldChars::CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const {
   return fon9_CompareBytes(rd.GetCellPtr<byte>(*this), this->Size_, rhs, rsz);
}

//--------------------------------------------------------------------------//

StrView FieldChar1::GetTypeId(NumOutBuf&) const {
   return StrView{"C1"};
}

void FieldChar1::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   if (char ch = this->GetValue(rd))
      FmtRevPrint(fmt, out, ch);
   else if (!fmt.empty())
      RevPrint(out, StrView{}, FmtDef{fmt});
}

OpResult FieldChar1::StrToCell(const RawWr& wr, StrView value) const {
   *wr.GetCellPtr<char>(*this) = value.empty() ? '\0' : *value.begin();
   return OpResult::no_error;
}

void FieldChar1::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   ToBitv(out, this->GetValue(rd));
}
OpResult FieldChar1::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   BitvTo(buf, *wr.GetCellPtr<char>(*this));
   return OpResult::no_error;
}

OpResult FieldChar1::Copy(const RawWr& wr, const RawRd& rd) const {
   *wr.GetCellPtr<char>(*this) = *rd.GetCellPtr<char>(*this);
   return OpResult::no_error;
}
int FieldChar1::Compare(const RawRd& lhs, const RawRd& rhs) const {
   return Compare2Values(*lhs.GetCellPtr<char>(*this), *rhs.GetCellPtr<char>(*this));
}
int FieldChar1::CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const {
   if (rsz) {
      int retval = Compare2Values(*rd.GetCellPtr<char>(*this), *static_cast<const char*>(rhs));
      if (retval || rsz == 1)
         return retval;
      return -1;
   }
   return 1; // rd > rhs(empty);
}

//--------------------------------------------------------------------------//

OpResult FieldEnabledYN::StrToCell(const RawWr& wr, StrView value) const {
   char ch = (toupper(value.Get1st()) == 'Y') ? 'Y' : '\0';
   return base::StrToCell(wr, StrView{&ch,1});
}
StrView FieldEnabledYN::GetTypeId(NumOutBuf&) const {
   return StrView{"C1Y"};
}
void FieldEnabledYN::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   ToBitv(out, this->GetValue(rd)=='Y');
}
OpResult FieldEnabledYN::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   bool res;
   BitvTo(buf, res);
   *wr.GetCellPtr<char>(*this) = res ? 'Y' : '\0';
   return OpResult::no_error;
}

} } // namespaces
