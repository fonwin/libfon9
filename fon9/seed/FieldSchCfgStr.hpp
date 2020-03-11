/// \file fon9/seed/FieldSchCfgStr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldSchCfgStr_hpp__
#define __fon9_seed_FieldSchCfgStr_hpp__
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/SchTask.hpp"

namespace fon9 { namespace seed {

struct SchCfgStr : public CharVector {
   using base = CharVector;
   using base::base;
   SchCfgStr() = default;

   void AssignCfgStr(const StrView& value) {
      SchConfig cfg{value};
      this->assign(ToStrView(RevPrintTo<CharVector>(cfg)));
   }
};

template <class BaseField>
class FieldSchCfgStrT : public BaseField {
   fon9_NON_COPY_NON_MOVE(FieldSchCfgStrT);
   using base = BaseField;

   void OnCellChanged(const RawWr& wr) const override {
      const StrView  bfValue = this->GetValue(wr);
      StrView        afValue = bfValue;
      CharVector     buf;
      if (!StrTrim(&afValue).empty()) {
         SchConfig cfg{afValue};
         afValue = ToStrView(RevPrintAppendTo(buf, cfg));
      }
      if (afValue != bfValue)
         this->StrToCellNoEvent(wr, afValue);
   }

public:
   using base::base;
};


/// \ingroup seed
/// 存取 SchConfig 設定字串.
class fon9_API FieldSchCfgStr : public FieldSchCfgStrT<FieldString<SchCfgStr::base> > {
   fon9_NON_COPY_NON_MOVE(FieldSchCfgStr);
   using base = FieldSchCfgStrT<FieldString<SchCfgStr::base> >;
public:
   using base::base;

   #define fon9_kCSTR_UDFieldMaker_SchCfgStr "Sch"
   StrView GetTypeId(NumOutBuf&) const override;
};

inline FieldSPT<FieldSchCfgStr> MakeField(Named&& named, int32_t ofs, SchCfgStr&) {
   return FieldSPT<FieldSchCfgStr>{new FieldSchCfgStr{std::move(named), ofs}};
}
inline FieldSPT<FieldSchCfgStr> MakeField(Named&& named, int32_t ofs, const SchCfgStr&) {
   return FieldSPT<FieldSchCfgStr>{new FieldConst<FieldSchCfgStr>{std::move(named), ofs}};
}

} } // namespaces
#endif//__fon9_seed_FieldSchCfgStr_hpp__
