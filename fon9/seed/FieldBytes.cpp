/// \file fon9/seed/FieldBytes.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldBytes.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/Base64.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace seed {

StrView FieldBytes::GetTypeId(NumOutBuf& buf) const {
   buf.SetEOS();
   char* pbeg = UIntToStrRev(buf.end(), this->Size_);
   *--pbeg = 'B';
   return StrView{pbeg, buf.end()};
}

void FieldBytes::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   (void)fmt;
   return RevPutB64(out, rd.GetCellPtr<byte>(*this), this->Size_);
}
OpResult FieldBytes::StrToCell(const RawWr& wr, StrView value) const {
   byte*  ptr = wr.GetCellPtr<byte>(*this);
   auto   r = Base64Decode(ptr, this->Size_, value.begin(), value.size());
   if (r.HasResult()) {
      size_t rsz = r.GetResult();
      if (rsz < this->Size_)
         memset(ptr + rsz, 0, this->Size_ - rsz);
      return OpResult::no_error;
   }
   if (r.GetError() == std::errc::no_buffer_space)
      return OpResult::value_overflow;
   memset(ptr, 0, this->Size_);
   return OpResult::value_format_error;
}
void FieldBytes::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   const byte* ptr = rd.GetCellPtr<byte>(*this);
   auto        count = this->Size_;
   while (count) {
      if (ptr[count - 1] != 0)
         break;
      --count;
   }
   ByteArrayToBitv(out, ptr, count);
}
OpResult FieldBytes::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   BitvToByteArray(buf, wr.GetCellPtr<byte>(*this), this->Size_);
   return OpResult::no_error;
}

FieldNumberT FieldBytes::GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const {
   (void)rd; (void)outDecScale;
   return nullValue;
}
OpResult FieldBytes::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   (void)wr; (void)num; (void)decScale;
   return OpResult::not_supported_number;
}

OpResult FieldBytes::SetNull(const RawWr& wr) const {
   memset(wr.GetCellPtr<byte>(*this), 0, this->Size_);
   return OpResult::no_error;
}
bool FieldBytes::IsNull(const RawRd& rd) const {
   const byte* ptr = rd.GetCellPtr<byte>(*this);
   for (auto L = this->Size_; L > 0; --L) {
      if (*ptr++ != 0)
         return false;
   }
   return true;
}

OpResult FieldBytes::Copy(const RawWr& wr, const RawRd& rd) const {
   memcpy(wr.GetCellPtr<byte>(*this), rd.GetCellPtr<byte>(*this), this->Size_);
   return OpResult::no_error;
}
int FieldBytes::Compare(const RawRd& lhs, const RawRd& rhs) const {
   return memcmp(lhs.GetCellPtr<byte>(*this), rhs.GetCellPtr<byte>(*this), this->Size_);
}
size_t FieldBytes::AppendRawBytes(const RawRd& rd, ByteVector& dst) const {
   dst.append(rd.GetCellPtr<byte>(*this), this->Size_);
   return this->Size_;
}
int FieldBytes::CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const {
   return fon9_CompareBytes(rd.GetCellPtr<byte>(*this), this->Size_, rhs, rsz);
}

//--------------------------------------------------------------------------//

StrView FieldByteVector::GetTypeId(NumOutBuf&) const {
   return StrView{"B0"};
}

void FieldByteVector::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   (void)fmt;
   const ByteVector& bv = rd.GetMemberCell<ByteVector>(*this);
   RevPutB64(out, bv.begin(), bv.size());
}
OpResult FieldByteVector::StrToCell(const RawWr& wr, StrView value) const {
   ByteVector& bv = wr.GetMemberCell<ByteVector>(*this);
   if (size_t bmax = Base64DecodeLength(value.size())) {
      if (void* mem = bv.alloc(bmax)) {
         auto r = Base64Decode(mem, bmax, value.begin(), value.size());
         bv.resize(static_cast<fon9_Blob_Size_t>(r.GetResult()));
      }
      else {
         bv.clear();
         return OpResult::mem_alloc_error;
      }
   }
   else
      bv.clear();
   return OpResult::no_error;
}

void FieldByteVector::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   ToBitv(out, rd.GetMemberCell<ByteVector>(*this));
}
OpResult FieldByteVector::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   BitvTo(buf, wr.GetMemberCell<ByteVector>(*this));
   return OpResult::no_error;
}

FieldNumberT FieldByteVector::GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const {
   (void)rd; (void)outDecScale;
   return nullValue;
}
OpResult FieldByteVector::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   (void)wr; (void)num; (void)decScale;
   return OpResult::not_supported_number;
}

OpResult FieldByteVector::SetNull(const RawWr& wr) const {
   wr.GetMemberCell<ByteVector>(*this).clear();
   return OpResult::no_error;
}
bool FieldByteVector::IsNull(const RawRd& rd) const {
   return rd.GetMemberCell<ByteVector>(*this).empty();
}

OpResult FieldByteVector::Copy(const RawWr& wr, const RawRd& rd) const {
   wr.GetMemberCell<ByteVector>(*this) = rd.GetMemberCell<ByteVector>(*this);
   return OpResult::no_error;
}
int FieldByteVector::Compare(const RawRd& lhs, const RawRd& rhs) const {
   return lhs.GetMemberCell<ByteVector>(*this).compare(rhs.GetMemberCell<ByteVector>(*this));
}
size_t FieldByteVector::AppendRawBytes(const RawRd& rd, ByteVector& dst) const {
   const auto& src = rd.GetMemberCell<ByteVector>(*this);
   const auto  sz = src.size();
   if (sz)
      dst.append(src.begin(), sz);
   return sz;
}
int FieldByteVector::CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const {
   const auto& lhs = rd.GetMemberCell<ByteVector>(*this);
   return fon9_CompareBytes(lhs.begin(), lhs.size(), rhs, rsz);
}

} } // namespaces
