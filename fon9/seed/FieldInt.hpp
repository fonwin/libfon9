/// \file fon9/seed/FieldInt.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldInt_hpp__
#define __fon9_seed_FieldInt_hpp__
#include "fon9/seed/FieldChars.hpp"
#include "fon9/Unaligned.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace seed {

template <bool isSigned, size_t sz> struct FieldIntTypeId;
template <> struct FieldIntTypeId<true, 1> { static constexpr StrView TypeId() { return StrView{"S1"}; } };
template <> struct FieldIntTypeId<true, 2> { static constexpr StrView TypeId() { return StrView{"S2"}; } };
template <> struct FieldIntTypeId<true, 4> { static constexpr StrView TypeId() { return StrView{"S4"}; } };
template <> struct FieldIntTypeId<true, 8> { static constexpr StrView TypeId() { return StrView{"S8"}; } };
template <> struct FieldIntTypeId<false, 1> { static constexpr StrView TypeId() { return StrView{"U1"}; } };
template <> struct FieldIntTypeId<false, 2> { static constexpr StrView TypeId() { return StrView{"U2"}; } };
template <> struct FieldIntTypeId<false, 4> { static constexpr StrView TypeId() { return StrView{"U4"}; } };
template <> struct FieldIntTypeId<false, 8> { static constexpr StrView TypeId() { return StrView{"U8"}; } };
template <typename IntT>
inline constexpr StrView GetFieldIntTypeId() {
   return FieldIntTypeId<std::is_signed<IntT>::value, sizeof(IntT)>::TypeId();
}

//--------------------------------------------------------------------------//

/// \ingroup seed
/// 存取[整數類型別]的欄位類別.
/// - Null = 0
template <class IntT>
class fon9_API FieldInt : public Field {
   fon9_NON_COPY_NON_MOVE(FieldInt);
   using base = Field;
   static_assert(std::is_integral<IntT>::value, "FieldInt<IntT>, IntT必須是整數型別");
protected:
   using base::base;

public:
   using OrigType = IntT;
   enum : IntT {
      kNullValue = IntT{}
   };

   /// 建構:
   /// - 固定為 f9sv_FieldType_Integer; FieldSource::DataMember;
   FieldInt(Named&& named, int32_t ofs)
      : base(std::move(named), f9sv_FieldType_Integer, FieldSource::DataMember, ofs, sizeof(IntT), 0) {
   }
   /// 建構:
   /// - 固定為 f9sv_FieldType_Integer; FieldSource::DyMem;
   FieldInt(Named&& named)
      : base(std::move(named), f9sv_FieldType_Integer, FieldSource::DyMem, 0, sizeof(IntT), 0) {
   }

   IntT GetValue(const RawRd& rd) const {
      IntT tmpValue;
      return rd.GetCellValue(*this, tmpValue);
   }
   void PutValue(const RawWr& wr, IntT value) const {
      wr.PutCellValue(*this, value);
   }

   static constexpr StrView TypeId() {
      return GetFieldIntTypeId<IntT>();
   }
   /// 傳回: "Sn" or "Un";  n = this->Size_;
   virtual StrView GetTypeId(NumOutBuf&) const override {
      return TypeId();
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      FmtRevPrint(fmt, out, this->GetValue(rd));
   }
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->PutValue(wr, StrTo(value, static_cast<IntT>(kNullValue)));
      return OpResult::no_error;
   }
   virtual void CellToBitv(const RawRd& rd, RevBuffer& out) const override {
      ToBitv(out, this->GetValue(rd));
   }
   virtual OpResult BitvToCell(const RawWr& wr, DcQueue& buf) const override {
      IntT val{};
      BitvTo(buf, val);
      this->PutValue(wr, val);
      return OpResult::no_error;
   }

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override {
      (void)nullValue;
      return static_cast<FieldNumberT>(AdjustDecScale(this->GetValue(rd), 0, outDecScale));
   }
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override {
      this->PutValue(wr, static_cast<IntT>(AdjustDecScale(num, decScale, 0)));
      return OpResult::no_error;
   }

   /// \retval true  整數值==kNullValue;
   virtual bool IsNull(const RawRd& rd) const override {
      return this->GetValue(rd) == static_cast<IntT>(kNullValue);
   }
   virtual OpResult SetNull(const RawWr& wr) const override {
      this->PutValue(wr, static_cast<IntT>(kNullValue));
      return OpResult::no_error;
   }

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override {
      this->PutValue(wr, this->GetValue(rd));
      return OpResult::no_error;
   }
   virtual int Compare(const RawRd& lhs, const RawRd& rhs) const override {
      return Compare2Values(this->GetValue(lhs), this->GetValue(rhs));
   }
   virtual size_t AppendRawBytes(const RawRd& rd, ByteVector& dst) const override {
      dst.append(rd.GetCellPtr<OrigType>(*this), sizeof(OrigType));
      return sizeof(OrigType);
   }
   virtual int CompareRawBytes(const RawRd& rd, const void* rhs, size_t rsz) const override {
      if (rsz == sizeof(OrigType)) {
         OrigType rval; memcpy(&rval, rhs, sizeof(OrigType));
         return Compare2Values(this->GetValue(rd), rval);
      }
      return 1;
   }
};

template <typename IntT, class FieldT = FieldInt<decay_t<IntT>>>
inline enable_if_t<std::is_integral<IntT>::value, FieldSPT<FieldT>>
MakeField(Named&& named, int32_t ofs, IntT&) {
   return FieldSPT<FieldT>{new FieldT{std::move(named), ofs}};
}

template <typename IntT, class FieldT = FieldInt<decay_t<IntT>>>
inline enable_if_t<std::is_integral<IntT>::value, FieldSPT<FieldT>>
MakeField(Named&& named, int32_t ofs, const IntT&) {
   return FieldSPT<FieldT>{new FieldConst<FieldT>{std::move(named), ofs}};
}

//--------------------------------------------------------------------------//

template <bool isSigned, size_t sz> struct FieldIntHexTypeId;
template <> struct FieldIntHexTypeId<true, 1> { static constexpr StrView TypeId() { return StrView{"S1x"}; } };
template <> struct FieldIntHexTypeId<true, 2> { static constexpr StrView TypeId() { return StrView{"S2x"}; } };
template <> struct FieldIntHexTypeId<true, 4> { static constexpr StrView TypeId() { return StrView{"S4x"}; } };
template <> struct FieldIntHexTypeId<true, 8> { static constexpr StrView TypeId() { return StrView{"S8x"}; } };
template <> struct FieldIntHexTypeId<false, 1> { static constexpr StrView TypeId() { return StrView{"U1x"}; } };
template <> struct FieldIntHexTypeId<false, 2> { static constexpr StrView TypeId() { return StrView{"U2x"}; } };
template <> struct FieldIntHexTypeId<false, 4> { static constexpr StrView TypeId() { return StrView{"U4x"}; } };
template <> struct FieldIntHexTypeId<false, 8> { static constexpr StrView TypeId() { return StrView{"U8x"}; } };
template <typename IntT>
inline constexpr StrView GetFieldIntHexTypeId() {
   return FieldIntHexTypeId<std::is_signed<IntT>::value, sizeof(IntT)>::TypeId();
}

template <typename IntT>
class FieldIntHex : public FieldInt<IntT> {
   fon9_NON_COPY_NON_MOVE(FieldIntHex);
   using base = FieldInt<IntT>;
public:
   using base::base;

   static constexpr StrView TypeId() {
      return GetFieldIntHexTypeId<IntT>();
   }
   /// 傳回: "Snx" or "Unx";  n = this->Size_;
   virtual StrView GetTypeId(NumOutBuf&) const override {
      return TypeId();
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      FmtRevPrint(fmt, out, ToxHex{this->GetValue(rd)});
   }
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->PutValue(wr, static_cast<IntT>(HIntStrTo(value)));
      return OpResult::no_error;
   }
};

/// \ingroup seed
/// 若 EnumT 有提供 bit op, 則使用 FieldIntHex 欄位.
/// 若 EnumT 沒提供 bit op, 則使用 FieldInt 欄位.
template <typename EnumT, typename IntT = typename std::underlying_type<EnumT>::type>
struct FieldEnumAux {
   using Field = conditional_t<HasBitOpT<EnumT>::value, FieldIntHex<IntT>, FieldInt<IntT>>;
   using FieldSP = FieldSPT<Field>;
};

/// \ingroup seed
/// 若 EnumT 為 enum : char, 則使用 FieldChar1 欄位.
template <typename EnumT>
struct FieldEnumAux<EnumT, char> {
   using Field = FieldChar1;
   using FieldSP = FieldSPT<Field>;
};

template <typename EnumT>
struct FieldEnumSelectorAux : public FieldEnumAux<EnumT> {
};

template <typename EnumT>
struct FieldEnumSelector : public conditional_t<std::is_enum<EnumT>::value, FieldEnumSelectorAux<EnumT>, std::false_type> {
};

template <typename EnumT, class Selector = FieldEnumSelector<EnumT>>
inline typename Selector::FieldSP MakeField(Named&& named, int32_t ofs, EnumT&) {
   return typename Selector::FieldSP{new typename Selector::Field{std::move(named), ofs}};
}

template <typename EnumT, class Selector = FieldEnumSelector<EnumT>>
inline typename Selector::FieldSP MakeField(Named&& named, int32_t ofs, const EnumT&) {
   return typename Selector::FieldSP{new FieldConst<typename Selector::Field>{std::move(named), ofs}};
}

//--------------------------------------------------------------------------//

template <bool isSigned, size_t sz> struct FieldIntHxTypeId;
template <> struct FieldIntHxTypeId<true,  1> { static constexpr StrView TypeId() { return StrView{"S1H"}; } };
template <> struct FieldIntHxTypeId<true,  2> { static constexpr StrView TypeId() { return StrView{"S2H"}; } };
template <> struct FieldIntHxTypeId<true,  4> { static constexpr StrView TypeId() { return StrView{"S4H"}; } };
template <> struct FieldIntHxTypeId<true,  8> { static constexpr StrView TypeId() { return StrView{"S8H"}; } };
template <> struct FieldIntHxTypeId<false, 1> { static constexpr StrView TypeId() { return StrView{"U1H"}; } };
template <> struct FieldIntHxTypeId<false, 2> { static constexpr StrView TypeId() { return StrView{"U2H"}; } };
template <> struct FieldIntHxTypeId<false, 4> { static constexpr StrView TypeId() { return StrView{"U4H"}; } };
template <> struct FieldIntHxTypeId<false, 8> { static constexpr StrView TypeId() { return StrView{"U8H"}; } };
template <typename IntT>
inline constexpr StrView GetFieldIntHxTypeId() {
   return FieldIntHxTypeId<std::is_signed<IntT>::value, sizeof(IntT)>::TypeId();
}

/// CellRevPrint(); StrToCell() 不支援 "0x" 或 "x" 前置; 一律使用 16 進位.
/// 不支援 MakeField(); 必須自行手動建立, 例如:
/// `new FieldIntHx<uint8_t>(fon9::Named{"OrdSt"}, fon9_OffsetOfRawPointer(OmsOrderRaw, OrderSt_));`
template <typename IntT>
class FieldIntHx : public FieldInt<IntT> {
   fon9_NON_COPY_NON_MOVE(FieldIntHx);
   using base = FieldInt<IntT>;
public:
   using base::base;

   static constexpr StrView TypeId() {
      return GetFieldIntHxTypeId<IntT>();
   }
   /// 傳回: "SnH" or "UnH";  n = this->Size_;
   virtual StrView GetTypeId(NumOutBuf&) const override {
      return TypeId();
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      FmtRevPrint(fmt, out, ToHex{this->GetValue(rd)});
   }
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->PutValue(wr, static_cast<IntT>(HexStrTo(value)));
      return OpResult::no_error;
   }
};

//--------------------------------------------------------------------------//

#define fon9_API_DEFINE_SEED_FIELDS(FieldT) \
template class FieldT<int8_t> fon9_API;     \
template class FieldT<int16_t> fon9_API;    \
template class FieldT<int32_t> fon9_API;    \
template class FieldT<int64_t> fon9_API;    \
template class FieldT<uint8_t> fon9_API;    \
template class FieldT<uint16_t> fon9_API;   \
template class FieldT<uint32_t> fon9_API;   \
template class FieldT<uint64_t> fon9_API;
//-------------------------------------------
fon9_API_DEFINE_SEED_FIELDS(FieldInt);
fon9_API_DEFINE_SEED_FIELDS(FieldIntHex);
fon9_API_DEFINE_SEED_FIELDS(FieldIntHx);

} } // namespaces
#endif//__fon9_seed_FieldInt_hpp__
