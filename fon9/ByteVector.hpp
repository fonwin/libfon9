/// \file fon9/ByteVector.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ByteVector_hpp__
#define __fon9_ByteVector_hpp__
#include "fon9/StrView.hpp"
#include "fon9/Blob.h"
#include <string.h>

namespace fon9 {

/// \ingroup Misc
/// 可變大小的 byte 陣列型別.
/// - 在小量資料時(在x64系統, 大約23 bytes), 可以不用分配記憶體.
/// - assign(); append(); 可能丟出 bad_alloc 異常.
/// - 尾端有 EOS, 但 EOS 不算在 size() 裡面.
class fon9_API ByteVector : public Comparable<ByteVector> {
protected:
   struct Key {
      union {
         fon9_Blob   Blob_;
         byte        InBuf_[sizeof(fon9_Blob)];
      };
      char  ExInBuf_[7];
      /// 為了讓使用內部記憶體時, 有 cstr(尾端有EOS) 的效果:
      /// - 所以 InLen_ = 0, 表示使用 InBuf_, 且長度為 kMaxInternalSize;
      /// - fon9_Blob 有提供 EOS.
      /// - InBuf_ 實際用量 = kMaxInternalSize + InLen_;
      char  InLen_;
      Key() {
         this->ForceEmpty();
      }
      void ForceEmpty() {
         this->InLen_ = -kMaxInternalSize;
         this->InBuf_[0] = '\0';
      }
      void SetUseRefOrBlob() {
         this->InLen_ = 1;
      }
      void SetRefMem(const void* mem, size_t sz) {
         this->SetUseRefOrBlob();
         this->Blob_.MemPtr_ = reinterpret_cast<byte*>(const_cast<void*>(mem));
         this->Blob_.MemSize_ = 0;
         this->Blob_.MemUsed_ = static_cast<fon9_Blob_Size_t>(sz);
      }
      bool IsUseRef() const {
         return(this->InLen_ > 0 && this->Blob_.MemSize_ == 0);
      }
      bool IsUseBlob() const {
         return(this->InLen_ > 0 && this->Blob_.MemSize_ > 0);
      }
      bool IsUseRefOrBlob() const {
         return(this->InLen_ > 0);
      }
      bool IsUseInternal() const {
         return(this->InLen_ <= 0);
      }
      size_t size() const {
         if (this->InLen_ <= 0)
            return static_cast<size_t>(kMaxInternalSize + this->InLen_);
         return this->Blob_.MemUsed_;
      }
      size_t GetInLen() const {
         assert(this->IsUseInternal());
         return static_cast<size_t>(kMaxInternalSize + this->InLen_);
      }
      void SetInLen(size_t sz) {
         assert(sz <= kMaxInternalSize);
         this->InLen_ = static_cast<char>(sz - kMaxInternalSize);
         this->InBuf_[sz] = '\0';
      }
      StrView ToStrView() const {
         return this->IsUseInternal()
            ? StrView{reinterpret_cast<const char*>(this->InBuf_), static_cast<size_t>(kMaxInternalSize + this->InLen_)}
            : StrView{reinterpret_cast<const char*>(this->Blob_.MemPtr_), this->Blob_.MemUsed_};
      }
   };
   Key   Key_;

   void FreeBlob() {
      if (this->Key_.IsUseBlob())
         fon9_Blob_Free(&this->Key_.Blob_);
   }
   /// 把現有資料放入 blob, 並額外保留 max(0, reversesz - this->size()) 個空間.
   /// assert(!this->IsUseBlob());
   bool MoveToBlob(size_t reversesz);

   void CopyBytes(const void* mem, size_t sz);

   /// 在 this 的存續期間, 須自行確保 mem 的有效!
   ByteVector(const void* mem, size_t sz) {
      this->Key_.SetRefMem(mem, sz);
   }

public:
   using value_type = byte;
   enum {
      kMaxInternalSize = offsetof(Key, InLen_)
   };

   ByteVector() = default;

   ByteVector(const ByteVector& rhs) {
      operator=(rhs);
   }
   ByteVector& operator=(const ByteVector& rhs);

   ByteVector(ByteVector&& rhs) {
      this->Key_ = rhs.Key_;
      rhs.Key_.ForceEmpty();
   }
   ByteVector& operator=(ByteVector&&);

   explicit ByteVector(StrView str) {
      this->CopyBytes(str.begin(), str.size());
   }
   explicit ByteVector(const std::string& str) {
      this->CopyBytes(str.c_str(), str.size());
   }
   ByteVector(const void* pbeg, const void* pend) {
      assert(pbeg <= pend);
      this->CopyBytes(pbeg, reinterpret_cast<uintptr_t>(pend) - reinterpret_cast<uintptr_t>(pbeg));
   }

   ~ByteVector() {
      this->FreeBlob();
   }

   static const ByteVector MakeRef(StrView str) {
      return ByteVector{str.begin(), str.size()};
   }
   static const ByteVector MakeRef(const std::string& str) {
      return ByteVector{str.c_str(), str.size()};
   }
   static const ByteVector MakeRef(const void* mem, size_t sz) {
      return ByteVector{mem,sz};
   }
   static const ByteVector MakeRef(const void* pbeg, const void* pend) {
      return ByteVector{pbeg,static_cast<size_t>(reinterpret_cast<const char*>(pend) - reinterpret_cast<const char*>(pbeg))};
   }

   int compare(const ByteVector& rhs) const {
      return fon9_CompareBytes(this->begin(), this->size(), rhs.begin(), rhs.size());
   }
   bool operator< (const ByteVector& rhs) const {
      return this->compare(rhs) < 0;
   }
   bool operator== (const ByteVector& rhs) const {
      size_t lsz = this->size();
      size_t rsz = rhs.size();
      return lsz == rsz && memcmp(this->begin(), rhs.begin(), rsz) == 0;
   }

   bool empty() const { return this->size() == 0; }
   size_t size() const { return this->Key_.size(); }

   byte* begin() {
      return this->Key_.IsUseInternal() ? this->Key_.InBuf_ : this->Key_.Blob_.MemPtr_;
   }
   byte* end() {
      if (this->Key_.IsUseInternal())
         return this->Key_.InBuf_ + this->Key_.GetInLen();
      return(this->Key_.Blob_.MemPtr_ + this->Key_.Blob_.MemUsed_);
   }
   const byte* begin() const {
      return const_cast<ByteVector*>(this)->begin();
   }
   const byte* end() const {
      return const_cast<ByteVector*>(this)->end();
   }
   const byte* cbegin() const {
      return this->begin();
   }
   const byte* cend() const {
      return this->end();
   }

   void clear();
   void reserve(size_t reversesz);
   void erase(size_t offset, size_t count);
   void Free() {
      if (this->Key_.IsUseBlob())
         fon9_Blob_Free(&this->Key_.Blob_);
      this->Key_.ForceEmpty();
   }

   void append(const void* mem, size_t sz);
   void append(const void* pbeg, const void* pend) {
      ptrdiff_t sz = reinterpret_cast<const byte*>(pend) - reinterpret_cast<const byte*>(pbeg);
      if (sz > 0)
         this->append(pbeg, static_cast<size_t>(sz));
   }
   void append(size_t sz, byte ch);
   void append(const StrView& src) {
      this->append(src.begin(), src.size());
   }

   void push_back(byte ch) {
      this->append(1, ch);
   }

   /// 分配記憶體,但不設定初值.
   /// 返回後應立即填入資料.
   /// - 若 sz == this->size() 則確定傳回 this->begin(); 不會重新分配(也不會釋放).
   ///   除非之前是用 MakeRef() 建立的.
   ///
   /// \retval nullptr  記憶體不足, 內容不變.
   /// \retval !nullptr 記憶體開始位置, 返回時 this->size()==sz, 內容無定義(原有資料不保留).
   void* alloc(size_t sz);

   void assign(const void* mem, size_t sz);
   void assign(const void* pbeg, const void* pend) {
      this->assign(pbeg, static_cast<size_t>(reinterpret_cast<const byte*>(pend) - reinterpret_cast<const byte*>(pbeg)));
   }
   void assign(const StrView& src) {
      this->assign(src.begin(), src.size());
   }

   size_t capacity() const {
      return this->Key_.IsUseBlob() ? this->Key_.Blob_.MemSize_ : static_cast<size_t>(this->kMaxInternalSize);
   }
   void resize(size_t sz);

   inline friend StrView CastToStrView(const ByteVector& bary) {
      return bary.Key_.ToStrView();
   }
};

} // namespaces
#endif//__fon9_ByteVector_hpp__
