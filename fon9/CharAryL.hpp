﻿/// \file fon9/CharAryL.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_CharAryL_hpp__
#define __fon9_CharAryL_hpp__
#include "fon9/CharAry.hpp"

namespace fon9 {

template <size_t sz, size_t bytes> struct LenTypeSelector;
template <> struct LenTypeSelector<0, 1> { using type = uint8_t; };
template <> struct LenTypeSelector<0, 2> { using type = uint16_t; };
template <> struct LenTypeSelector<0, 3> { using type = uint32_t; };
template <> struct LenTypeSelector<0, 4> { using type = uint32_t; };
template <size_t sz, size_t bytes = 0>
struct LenTypeSelector : public LenTypeSelector<sz / 0x100, bytes + 1> {
};

static_assert(sizeof(LenTypeSelector<1>::type) == 1, "");
static_assert(sizeof(LenTypeSelector<255>::type) == 1, "");
static_assert(sizeof(LenTypeSelector<256>::type) == 2, "");
static_assert(sizeof(LenTypeSelector<65535>::type) == 2, "");
static_assert(sizeof(LenTypeSelector<65536>::type) == 4, "");

/// \ingroup AlNum
/// 提供固定大小, 且攜帶一個「長度」欄位的字串。
template <size_t arysz, typename CharT = char>
class CharAryL : public Comparable<CharAryL<arysz, CharT>> {
   using Ary = CharAry<arysz, CharT>;
   using LenT = typename LenTypeSelector<arysz>::type;
protected:
   Ary   Ary_;
   LenT  Len_;

public:
   using value_type = CharT;
   using size_type = LenT;

   CharAryL() : Len_{0} {
   }
   CharAryL(const CharT* src, size_t srcsz) {
      this->CopyFrom(src, srcsz);
   }
   CharAryL(const StrView& str) {
      this->CopyFrom(str);
   }
   template <size_t srcsz>
   CharAryL(const CharT(&src)[srcsz]) {
      this->CopyFrom(src, srcsz - (src[srcsz - 1] == 0));
   }
   template <size_t srcsz>
   CharAryL(CharT(&src)[srcsz]) = delete;

   void CopyFrom(const CharT* src, size_t srcsz) {
      this->Len_ = static_cast<LenT>(srcsz < arysz ? srcsz : arysz);
      memcpy(this->Ary_.Chars_, src, this->Len_ * sizeof(CharT));
   }
   void CopyFrom(const StrView& str) {
      this->CopyFrom(str.begin(), str.size());
   }

   CharAryL(CharT chFiller) : Len_{0} {
      static_assert(sizeof(chFiller) == 1, "CharAryL: cannot use memset()");
      memset(this->Ary_.Chars_, chFiller, arysz);
   }
   CharAryL(const CharT* src, size_t srcsz, CharT chFiller) {
      this->CopyFrom(src, srcsz, chFiller);
   }
   CharAryL(const StrView& str, CharT chFiller) {
      this->CopyFrom(str.begin(), str.size(), chFiller);
   }
   template <size_t srcsz>
   CharAryL(const CharT(&src)[srcsz], CharT chFiller) {
      this->CopyFrom(src, srcsz - (src[srcsz - 1] == 0), chFiller);
   }
   template <size_t srcsz>
   CharAryL(CharT(&src)[srcsz], CharT chFiller) = delete;

   void CopyFrom(const CharT* src, size_t srcsz, CharT chFiller) {
      static_assert(sizeof(chFiller) == 1, "CharAryL: cannot use memset()");
      if (srcsz >= arysz)
         memcpy(this->Ary_.Chars_, src, this->Len_ = arysz);
      else {
         memcpy(this->Ary_.Chars_, src, this->Len_ = static_cast<LenT>(srcsz));
         memset(this->Ary_.Chars_ + srcsz, chFiller, arysz - srcsz);
      }
   }

   template <size_t sz, CharT kFiller>
   CharAry<sz, CharT, kFiller>& ToCharAry() {
      static_assert(sz <= arysz, "CharAryL::ToCharAry<sz>(); sz too big.");
      if (sz > this->Len_)
         memset(this->Ary_.Chars_, kFiller, sz - this->Len_);
      return *reinterpret_cast<CharAry<sz, CharT, kFiller>*>(&this->Ary_);
   }

   /// 使用 ToStrView() 來比較.
   int Compare(const CharAryL& rhs) const {
      return ToStrView(*this).Compare(ToStrView(rhs));
   }
   bool operator<(const CharAryL& rhs) const {
      return this->Compare(rhs) < 0;
   }
   bool operator==(const CharAryL& rhs) const {
      return this->Len_ == rhs.Len_ && memcmp(this->Ary_.Chars_, rhs.Ary_.Chars_, this->Len_) == 0;
   }

   const CharT* cbegin() const { return this->Ary_.begin(); }
   const CharT* cend()   const { return this->begin() + this->Len_; }
   const CharT* begin()  const { return this->cbegin(); }
   const CharT* end()    const { return this->cend(); }
   CharT*       begin()        { return this->Ary_.begin(); }
   CharT*       end()          { return this->begin() + this->Len_; }

   const CharT* data() const { return this->Ary_.data(); }
   CharT*       data()       { return this->Ary_.data(); }
   const CharT& operator[](size_t idx) const { assert(idx < this->Len_); return this->Ary_.Chars_[idx]; }
   CharT&       operator[](size_t idx)       { assert(idx < this->Len_); return this->Ary_.Chars_[idx]; }

   void clear() { this->Len_ = 0; }

   constexpr bool empty() const { return this->Len_ == 0; }

   const CharT& back() const { assert(this->Len_ > 0); return this->Ary_.Chars_[this->Len_ - 1]; }
   CharT&       back()       { assert(this->Len_ > 0); return this->Ary_.Chars_[this->Len_ - 1]; }
   void         pop_back()   { assert(this->Len_ > 0); --this->Len_; }
   void         push_back(CharT ch) { assert(this->Len_ < arysz); this->Ary_.Chars_[this->Len_++] = ch; }

          constexpr size_type size() const { return this->Len_; }
   static constexpr size_type max_size()   { return arysz; }
   static constexpr size_t    capacity()   { return arysz; }

   friend StrView ToStrView(const CharAryL& pthis) {
      return StrView{pthis.begin(), pthis.end()};
   }
};

} // namespace fon9
#endif//__fon9_CharAryL_hpp__
