/// \file fon9/CharVector.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_CharVector_hpp__
#define __fon9_CharVector_hpp__
#include "fon9/ByteVector.hpp"

namespace fon9 {

/// \ingroup Misc
/// - 當 key 使用時, 可用來取代 std::string
/// - 在資料量 <= ByteVector::kMaxInternalSize 時, 不用分配記憶體.
struct CharVector : public ByteVector {
   using value_type = char;
   using ByteVector::ByteVector;

   static const CharVector MakeRef(const StrView& str) {
      return CharVector{str.begin(), str.size()};
   }
   static const CharVector MakeRef(const std::string& str) {
      return CharVector{str.c_str(), str.size()};
   }
   static const CharVector MakeRef(const void* mem, size_t sz) {
      return CharVector{mem,sz};
   }
   static const CharVector MakeRef(const void* pbeg, const void* pend) {
      return CharVector{pbeg,static_cast<size_t>(reinterpret_cast<const char*>(pend) - reinterpret_cast<const char*>(pbeg))};
   }

   using ByteVector::append;
   void append(size_t sz, char ch) {
      ByteVector::append(sz, static_cast<byte>(ch));
   }
   void push_back(char ch) {
      this->append(1, ch);
   }

   const char* begin() const { return reinterpret_cast<const char*>(ByteVector::begin()); }
   const char* end() const { return reinterpret_cast<const char*>(ByteVector::end()); }
   char* begin() { return reinterpret_cast<char*>(ByteVector::begin()); }
   char* end() { return reinterpret_cast<char*>(ByteVector::end()); }

   std::string& AppendTo(std::string& out) const {
      this->Key_.ToStrView().AppendTo(out);
      return out;
   }
   std::string ToString() const {
      return this->Key_.ToStrView().ToString();
   }
   inline friend StrView ToStrView(const CharVector& pthis) {
      return pthis.Key_.ToStrView();
   }
};
StrView ToStrView(const CharVector& pthis);

struct CharVectorComparer {
   inline bool operator()(const StrView& lhs, const CharVector& rhs) const {
      return lhs < ToStrView(rhs);
   }
   inline bool operator()(const CharVector& lhs, const StrView& rhs) const {
      return ToStrView(lhs) < rhs;
   }
   inline bool operator()(const CharVector& lhs, const CharVector& rhs) const {
      return lhs < rhs;
   }
};

} // namespaces

namespace std {
template<>
struct hash<fon9::CharVector> : public hash<fon9::StrView> {
   size_t operator()(const fon9::CharVector& val) const {
      return hash<fon9::StrView>::operator()(fon9::ToStrView(val));
   }
};
}
#endif//__fon9_CharVector_hpp__
