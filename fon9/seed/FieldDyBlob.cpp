/// \file fon9/seed/FieldDyBlob.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldDyBlob.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/Base64.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace seed {

StrView FieldDyBlob::GetTypeId(NumOutBuf&) const {
   return this->Type_ == f9sv_FieldType_Chars ? StrView{"C0"} : StrView{"B0"};
}

void FieldDyBlob::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   const fon9_Blob blob = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
   if (blob.MemUsed_ == 0) {
      if (!fmt.empty())
         RevPrint(out, StrView{}, FmtDef{fmt});
      return;
   }
   if (this->Type_ == f9sv_FieldType_Chars)
      FmtRevPrint(fmt, out, StrView{reinterpret_cast<char*>(blob.MemPtr_), blob.MemUsed_});
   else
      RevPutB64(out, blob.MemPtr_, blob.MemUsed_);
}

void FieldDyBlob::OnCellChanged(const RawWr&) const {
}
OpResult FieldDyBlob::StrToCellNoEvent(const RawWr& wr, StrView value) const {
   fon9_Blob* ptr = wr.GetCellPtr<fon9_Blob>(*this);
   fon9_Blob blob = GetUnaligned(ptr);
   if (this->Type_ == f9sv_FieldType_Chars)
      fon9_Blob_Set(&blob, value.begin(), static_cast<fon9_Blob_Size_t>(value.size()));
   else if (size_t bmax = Base64DecodeLength(value.size())) {
      if (fon9_UNLIKELY(bmax != static_cast<fon9_Blob_Size_t>(bmax)
                        || !fon9_Blob_Set(&blob, nullptr, static_cast<fon9_Blob_Size_t>(bmax)))) {
         fon9_Blob_Free(&blob);
         PutUnaligned(ptr, blob);
         return OpResult::mem_alloc_error;
      }
      auto r = Base64Decode(blob.MemPtr_, bmax, value.begin(), value.size());
      blob.MemUsed_ = static_cast<fon9_Blob_Size_t>(r.GetResult());
   }
   else
      blob.MemUsed_ = 0;
   PutUnaligned(ptr, blob);
   return OpResult::no_error;
}
OpResult FieldDyBlob::StrToCell(const RawWr& wr, StrView value) const {
   OpResult res = this->StrToCellNoEvent(wr, value);
   if (res == OpResult::no_error)
      this->OnCellChanged(wr);
   return res;
}

void FieldDyBlob::CellToBitv(const RawRd& rd, RevBuffer& out) const {
   const fon9_Blob blob = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
   ByteArrayToBitv(out, blob.MemPtr_, blob.MemUsed_);
}

OpResult FieldDyBlob::BitvToCell(const RawWr& wr, DcQueue& buf) const {
   fon9_Blob* ptr = wr.GetCellPtr<fon9_Blob>(*this);
   fon9_Blob  blob = GetUnaligned(ptr);
   BitvTo(buf, blob);
   PutUnaligned(ptr, blob);
   this->OnCellChanged(wr);
   return OpResult::no_error;
}

FieldNumberT FieldDyBlob::GetNumber(const RawRd&, DecScaleT, FieldNumberT nullValue) const {
   return nullValue;
}
OpResult FieldDyBlob::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   (void)wr; (void)num; (void)decScale;
   return OpResult::not_supported_number;
}

OpResult FieldDyBlob::SetNull(const RawWr& wr) const {
   fon9_Blob* blob = wr.GetCellPtr<fon9_Blob>(*this);
   PutUnaligned(&blob->MemUsed_, static_cast<fon9_Blob_Size_t>(0));
   return OpResult::no_error;
}
bool FieldDyBlob::IsNull(const RawRd& rd) const {
   const fon9_Blob* blob = rd.GetCellPtr<fon9_Blob>(*this);
   return GetUnaligned(&blob->MemUsed_) == 0;
}

OpResult FieldDyBlob::Copy(const RawWr& wr, const RawRd& rd) const {
   fon9_Blob dst = GetUnaligned(wr.GetCellPtr<fon9_Blob>(*this));
   fon9_Blob src = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
   if (src.MemUsed_ > 0)
      fon9_Blob_Set(&dst, src.MemPtr_, src.MemUsed_);
   else
      dst.MemUsed_ = 0;
   wr.PutCellValue(*this, dst);
   return OpResult::no_error;
}
int FieldDyBlob::Compare(const RawRd& lhs, const RawRd& rhs) const {
   fon9_Blob L = GetUnaligned(lhs.GetCellPtr<fon9_Blob>(*this));
   fon9_Blob R = GetUnaligned(rhs.GetCellPtr<fon9_Blob>(*this));
   return fon9_CompareBytes(L.MemPtr_, L.MemUsed_, R.MemPtr_, R.MemUsed_);
}
size_t FieldDyBlob::AppendRawBytes(const RawRd& rd, ByteVector& dst) const {
   const fon9_Blob  src = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
   dst.append(src.MemPtr_, src.MemUsed_);
   return src.MemUsed_;
}
int FieldDyBlob::CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const {
   const fon9_Blob lhs = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
   return fon9_CompareBytes(lhs.MemPtr_, lhs.MemUsed_, rhs, rsz);
}

//--------------------------------------------------------------------------//

void FieldStrEnum::EnumList::RebuildEnumList(StrView cfgs) {
   this->OrigEnumStr_ = cfgs.ToString();
   cfgs = &this->OrigEnumStr_;
   this->IdEnumMap_.clear();
   StrView tag = SbrTrimHeadFetchInside(cfgs);
   if (!tag.IsNull())
      cfgs = tag;
   StrView val;
   while (SbrFetchTagValue(cfgs, tag, val))
      this->IdEnumMap_.kfetch(tag).second = val;
}

void FieldStrEnum::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   (void)fmt;
   RevPrint(out, this->EnumList_.GetEnumValue(this->GetValue(rd)));
}

} } // namespaces
