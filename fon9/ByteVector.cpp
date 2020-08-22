/// \file fon9/ByteVector.cpp
/// \author fonwinz@gmail.com
#include "fon9/ByteVector.hpp"
#include "fon9/Utility.hpp"
#include "fon9/Exception.hpp"

namespace fon9 {

void ByteVector::CopyBytes(const void* mem, size_t sz) {
   if (sz <= this->kMaxInternalSize) {
      this->FreeBlob();
      if (sz <= 0)
         this->Key_.ForceEmpty();
      else {
         this->Key_.SetInLen(sz);
         memcpy(this->Key_.InBuf_, mem, sz);
      }
   }
   else {
      assert(static_cast<fon9_Blob_Size_t>(sz) == sz);
      if (!this->Key_.IsUseBlob()) {
         fon9::ForceZeroNonTrivial(this->Key_);
         this->Key_.SetUseRefOrBlob();
      }
      fon9_Blob_Set(&this->Key_.Blob_, mem, static_cast<fon9_Blob_Size_t>(sz));
   }
}

ByteVector& ByteVector::operator=(const ByteVector& rhs) {
   if (this == &rhs)
      return *this;
   if (rhs.Key_.IsUseInternal()) {
      this->FreeBlob();
      this->Key_ = rhs.Key_;
   }
   else
      this->CopyBytes(rhs.Key_.Blob_.MemPtr_, rhs.Key_.Blob_.MemUsed_);
   return *this;
}

ByteVector& ByteVector::operator=(ByteVector&& rhs) {
   if (this == &rhs)
      return *this;
   if (rhs.Key_.IsUseRef())
      this->CopyBytes(rhs.Key_.Blob_.MemPtr_, rhs.Key_.Blob_.MemUsed_);
   else {
      this->FreeBlob();
      this->Key_ = rhs.Key_;
   }
   rhs.Key_.ForceEmpty();
   return *this;
}

bool ByteVector::MoveToBlob(size_t reversesz) {
   assert(!this->Key_.IsUseBlob());
   fon9_Blob  blob;
   ZeroStruct(blob);
   const size_t oldsz = this->size();
   if (reversesz < oldsz)
      reversesz = oldsz;
   if (!fon9_Blob_Set(&blob, nullptr, static_cast<fon9_Blob_Size_t>(reversesz)))
      return false;
   if (oldsz > 0)
      memcpy(blob.MemPtr_, this->begin(), oldsz);
   blob.MemUsed_ = static_cast<fon9_Blob_Size_t>(oldsz);
   this->Key_.SetUseRefOrBlob();
   this->Key_.Blob_ = blob;
   return true;
}

void ByteVector::erase(size_t offset, size_t count) {
   if (fon9_UNLIKELY(this->Key_.IsUseRef()))
      this->CopyBytes(this->Key_.Blob_.MemPtr_, this->Key_.Blob_.MemUsed_);
   size_t cursz = this->size();
   if (fon9_UNLIKELY(offset >= cursz))
      return;
   if (offset + count >= cursz)
      cursz = offset;
   else {
      byte*  beg = this->begin() + offset;
      cursz -= count;
      memmove(beg, beg + count, cursz - offset);
   }
   if (this->Key_.IsUseBlob())
      this->Key_.Blob_.MemUsed_ = static_cast<fon9_Blob_Size_t>(cursz);
   else {
      assert(this->Key_.IsUseInternal());
      this->Key_.SetInLen(cursz);
   }
}

void ByteVector::resize(size_t newsz) {
   size_t cursz = this->size();
   if (cursz == newsz)
      return;
   if (cursz < newsz)
      this->append(newsz - cursz, '\0');
   else if (this->Key_.IsUseRefOrBlob())
      this->Key_.Blob_.MemUsed_ = static_cast<fon9_Blob_Size_t>(newsz);
   else
      this->Key_.SetInLen(newsz);
}

void ByteVector::clear() {
   if (this->Key_.IsUseBlob())
      this->Key_.Blob_.MemUsed_ = 0;
   else
      this->Key_.ForceEmpty();
}

void* ByteVector::alloc(size_t sz) {
   if (sz <= this->kMaxInternalSize) {
      this->FreeBlob();
      this->Key_.SetInLen(sz);
      return this->Key_.InBuf_;
   }
   if(this->Key_.IsUseBlob()) {
      if (this->Key_.Blob_.MemSize_ >= sz) {
         this->Key_.Blob_.MemUsed_ = static_cast<fon9_Blob_Size_t>(sz);
         return this->Key_.Blob_.MemPtr_;
      }
   }
   if (static_cast<fon9_Blob_Size_t>(sz) != sz) // size_t sz; => fon9_Blob_Size_t; 容納不下!
      return nullptr;
   fon9_Blob  blob;
   ZeroStruct(blob);
   if (!fon9_Blob_Set(&blob, nullptr, static_cast<fon9_Blob_Size_t>(sz)))
      return nullptr;
   this->FreeBlob();
   this->Key_.SetUseRefOrBlob();
   this->Key_.Blob_ = blob;
   return this->Key_.Blob_.MemPtr_;
}

void ByteVector::assign(const void* mem, size_t sz) {
   byte* pbeg = this->begin();
   if (fon9_UNLIKELY(mem == pbeg)) {
      assert(sz <= this->size());
   }
   else {
      byte* pend = this->end();
      if (fon9_UNLIKELY(pbeg < reinterpret_cast<const byte*>(mem)
                         && reinterpret_cast<const byte*>(mem) < pend)) {
         assert(reinterpret_cast<const byte*>(mem) + sz <= pend);
         if (this->Key_.IsUseRef()) {
            this->Key_.Blob_.MemPtr_  = reinterpret_cast<byte*>(const_cast<void*>(mem));
            this->Key_.Blob_.MemUsed_ = static_cast<fon9_Blob_Size_t>(sz);
            return;
         }
         memmove(pbeg, mem, sz);
      }
      else { // 一般情況: 若 mem 不在 [beg..end) 之間, 則走到這裡.
         if (void* ptr = this->alloc(sz)) {
            memcpy(ptr, mem, sz);
            return;
         }
         Raise<std::bad_alloc>();
      }
   }
   // 若 mem 在 [beg..end) 之間, 則在 memmove() 之後, 走到這裡, 調整大小.
   if (this->Key_.IsUseBlob())
      this->Key_.Blob_.MemUsed_ = static_cast<fon9_Blob_Size_t>(sz);
   else
      this->Key_.SetInLen(sz);
}

void ByteVector::reserve(size_t reversesz) {
   if (reversesz <= this->kMaxInternalSize)
      return;
   if (static_cast<fon9_Blob_Size_t>(reversesz) != reversesz)
      return;
   if (!this->Key_.IsUseBlob())
      this->MoveToBlob(reversesz);
   else if (reversesz > this->Key_.Blob_.MemSize_)
      fon9_Blob_Append(&this->Key_.Blob_, nullptr, static_cast<fon9_Blob_Size_t>(reversesz - this->Key_.Blob_.MemSize_));
}

void ByteVector::append(size_t sz, byte ch) {
   if (!this->Key_.IsUseBlob()) {
      const size_t oldsz = this->size();
      const size_t newsz = oldsz + sz;
      if (newsz <= this->kMaxInternalSize) {
         if (fon9_LIKELY(newsz > 0)) {
            if (fon9_UNLIKELY(this->Key_.IsUseRefOrBlob())) // this 使用 MakeRef() 所建立.
               memcpy(this->Key_.InBuf_, this->Key_.Blob_.MemPtr_, oldsz);
            memset(this->Key_.InBuf_ + oldsz, ch, sz);
            this->Key_.SetInLen(newsz);
         }
         return;
      }
      if (!this->MoveToBlob(newsz))
         goto __MEM_ERR;
   }
   if (!fon9_Blob_Append(&this->Key_.Blob_, nullptr, static_cast<fon9_Blob_Size_t>(sz))) {
   __MEM_ERR:;
      Raise<std::bad_alloc>();
      return;
   }
   memset(this->Key_.Blob_.MemPtr_ + this->Key_.Blob_.MemUsed_, ch, sz);
   this->Key_.Blob_.MemPtr_[this->Key_.Blob_.MemUsed_ += static_cast<fon9_Blob_Size_t>(sz)] = '\0';
}
void ByteVector::append(const void* mem, size_t sz) {
   if (!this->Key_.IsUseBlob()) {
      const size_t oldsz = this->size();
      const size_t newsz = oldsz + sz;
      if (newsz <= this->kMaxInternalSize) {
         if (fon9_LIKELY(newsz > 0)) {
            if (fon9_UNLIKELY(this->Key_.IsUseRefOrBlob())) // this 使用 MakeRef() 所建立.
               memcpy(this->Key_.InBuf_, this->Key_.Blob_.MemPtr_, oldsz);
            memcpy(this->Key_.InBuf_ + oldsz, mem, sz);
            this->Key_.SetInLen(newsz);
         }
         return;
      }
      if (!this->MoveToBlob(newsz))
         goto __MEM_ERR;
   }
   if (!fon9_Blob_Append(&this->Key_.Blob_, mem, static_cast<fon9_Blob_Size_t>(sz))) {
   __MEM_ERR:;
      Raise<std::bad_alloc>();
   }
}

} // namespaces
