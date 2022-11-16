/// \file fon9/seed/FieldString.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldString_hpp__
#define __fon9_seed_FieldString_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 存取 StringT member 的欄位.
/// \tparam StringT 必須支援:
///   - StrView ToStrView(const StringT&);
///   - StringT::clear();
///   - StringT::assign(const char* pbeg, const char* pend);
///   - bool StringT::empty() const;
///   - StringT& StringT::operator=(const StringT&);
template <class StringT>
class fon9_API FieldString : public Field {
   fon9_NON_COPY_NON_MOVE(FieldString);
   typedef Field  base;

protected:
   virtual void OnCellChanged(const RawWr&) const {
   }
   OpResult StrToCellNoEvent(const RawWr& wr, StrView value) const {
      wr.GetMemberCell<StringT>(*this).assign(value.begin(), value.end());
      return OpResult::no_error;
   }

public:
   using ValueT = StringT;

   /// 建構, 固定為:
   /// - f9sv_FieldType_Chars;
   /// - StringT 只能是 data member(不可是 DyMem), 所以 FieldSource 固定為 DataMember;
   FieldString(Named&& named, int32_t ofs)
      : base{std::move(named), f9sv_FieldType_Chars, FieldSource::DataMember, ofs, sizeof(StringT), 0} {
   }

   StrView GetValue(const RawRd& rd) const {
      return ToStrView(rd.GetMemberCell<StringT>(*this));
   }
   StrView GetStrView(const RawRd& rd) const {
      return this->GetValue(rd);
   }

   /// 傳回: "C0";
   virtual StrView GetTypeId(NumOutBuf&) const override {
      return StrView{"C0"};
   }
   virtual uint32_t GetAvailSize() const override {
      return 0;
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      FmtRevPrint(fmt, out, this->GetValue(rd));
   }

   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->StrToCellNoEvent(wr, value);
      this->OnCellChanged(wr);
      return OpResult::no_error;
   }

   virtual void CellToBitv(const RawRd& rd, RevBuffer& out) const override {
      ToBitv(out, this->GetValue(rd));
   }
   virtual OpResult BitvToCell(const RawWr& wr, DcQueue& buf) const override {
      BitvTo(buf, wr.GetMemberCell<StringT>(*this));
      this->OnCellChanged(wr);
      return OpResult::no_error;
   }

   virtual OpResult SetNull(const RawWr& wr) const override {
      wr.GetMemberCell<StringT>(*this).clear();
      return OpResult::no_error;
   }

   virtual bool IsNull(const RawRd& rd) const override {
      return rd.GetMemberCell<StringT>(*this).empty();
   }

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override {
      return StrToDec(this->GetValue(rd), outDecScale, nullValue);
   }

   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override {
      NumOutBuf nbuf;
      char*     pbeg = DecToStrRev(nbuf.end(), num, decScale);
      wr.GetMemberCell<StringT>(*this).assign(pbeg, nbuf.end());
      return OpResult::no_error;
   }

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override {
      wr.GetMemberCell<StringT>(*this) = rd.GetMemberCell<StringT>(*this);
      return OpResult::no_error;
   }
   virtual int Compare(const RawRd& lhs, const RawRd& rhs) const override {
      return lhs.GetMemberCell<StringT>(*this).compare(rhs.GetMemberCell<StringT>(*this));
   }
   virtual size_t AppendRawBytes(const RawRd& rd, ByteVector& dst) const override {
      const auto src = ToStrView(rd.GetMemberCell<StringT>(*this));
      dst.append(src);
      return src.size();
   }
   virtual int CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const override {
      const auto lhs = ToStrView(rd.GetMemberCell<StringT>(*this));
      return fon9_CompareBytes(lhs.begin(), lhs.size(), rhs, rsz);
   }
};

template <class StringT, class FieldT = FieldString<decay_t<StringT>>>
inline auto MakeField(Named&& named, int32_t ofs, StringT& dataMember)
-> decltype(ToStrView(dataMember), dataMember.reserve(0), FieldSPT<FieldT>{}) {
   (void)dataMember;
   return FieldSPT<FieldT>{new FieldT{std::move(named), ofs}};
}

template <class StringT, class FieldT = FieldString<decay_t<StringT>>>
inline auto MakeField(Named&& named, int32_t ofs, const StringT& dataMember)
-> decltype(ToStrView(dataMember), const_cast<StringT*>(&dataMember)->reserve(0), FieldSPT<FieldT>{}) {
   (void)dataMember;
   return FieldSPT<FieldT>{new FieldConst<FieldT>{std::move(named), ofs}};
}

/// \ingroup seed
/// FieldStdString = 存取 std::string member 的欄位.
fon9_API_TEMPLATE_CLASS(FieldStdString, FieldString, std::string)

/// \ingroup seed
/// FieldCharVector = 存取 CharVector member 的欄位.
fon9_API_TEMPLATE_CLASS(FieldCharVector, FieldString, CharVector)

} } // namespaces

namespace fon9 {

// 因為 EvStringT<> 有 friend void BitvTo(); friend StrView ToStrView
// 所以 EvStringT<> 放到 namespace fon9;
template <class StringT>
class EvStringT {
protected:
   StringT  String_;
   virtual void OnStringAfterChanged() = 0;
public:
   virtual ~EvStringT() = default;

   bool empty() const {
      return this->String_.empty();
   }
   int compare(const EvStringT& rhs) const {
      return this->String_.compare(rhs.String_);
   }
   void assign(const char* pbeg, const char* pend) {
      this->String_.assign(pbeg, pend);
      this->OnStringAfterChanged();
   }
   void clear() {
      this->String_.clear();
      this->OnStringAfterChanged();
   }

   inline friend void BitvTo(DcQueue& dcq, EvStringT& dst) {
      BitvTo(dcq, dst.String_);
      dst.OnStringAfterChanged();
   }
   inline friend StrView ToStrView(const EvStringT& src) {
      return ToStrView(src.String_);
   }
};
fon9_API_TEMPLATE_CLASS(EvCharVector, EvStringT, CharVector)
fon9_API_TEMPLATE_CLASS(EvStdString, EvStringT, std::string)

namespace seed { // MakeField() 必須在 namespace fon9::seed;
template <class StringT>
inline FieldSP MakeField(Named&& named, int32_t ofs, EvStringT<StringT>&) {
   using FieldT = FieldString<EvStringT<StringT>>;
   return FieldSP{new FieldT{std::move(named), ofs}};
}
template <class StringT>
inline FieldSP MakeField(Named&& named, int32_t ofs, const EvStringT<StringT>&) {
   using FieldT = FieldString<EvStringT<StringT>>;
   return FieldSP{new seed::FieldConst<FieldT>{std::move(named), ofs}};
}

} } // namespaces
#endif//__fon9_seed_FieldString_hpp__
