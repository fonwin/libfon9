// \file fon9/InnStream.cpp
// \author fonwinz@gmail.com
#include "fon9/InnStream.hpp"

namespace fon9 {

InnStream& InnStream::operator=(InnStream&& src) {
   ImplBase  srcImpl;
   InnFileP* ownerInnFile;
   {
      Locker srcLocker{src.Lock()};
      src.Flush(srcLocker);
      ownerInnFile = src.OwnerInnFile_;
      srcImpl = std::move(*srcLocker);
      src.Close(srcLocker);
   }
   Locker thisLocker{this->Lock()};
   this->Close(thisLocker);
   this->OwnerInnFile_ = ownerInnFile;
   *thisLocker = std::move(srcImpl);
   return *this;
}
InnStream::~InnStream() {
   this->Close();
}
void InnStream::Close(const Locker& impl) {
   this->Flush(impl);
   impl->Close();
}
void InnStream::Flush(const Locker& impl) {
   (void)impl;
}
void InnStream::OnFirstRoomPosChanged(const Locker& impl, RoomPos newFirstRoomPos) {
   (void)impl; (void)newFirstRoomPos;
}
void InnStream::Open(InnFileP& owner, RoomPos startAt, const OpenArgs& openArgs) {
   Locker impl{this->Lock()};
   this->Close(impl);
   this->OwnerInnFile_ = &owner;
   impl->OpenArgs_ = openArgs;
   impl->NextRoomPos_ = startAt;
   InnRoomSize lastRoomSize = 1024;
   for (auto& rsz : impl->OpenArgs_.ExpectedRoomSize_) {
      if (rsz == 0)
         rsz = lastRoomSize;
      else
         lastRoomSize = rsz;
   }
}
InnStream::RoomPos InnStream::GetFirstRoomPos() const {
   Impl::ConstLocker impl{this->Impl_};
   if (this->OwnerInnFile_ == nullptr)
      return 0u;
   if (impl->BlockList_.empty())
      return impl->NextRoomPos_;
   return impl->BlockList_[0].RoomKey_.GetRoomPos();
}
//--------------------------------------------------------------------------//
void InnStream::ImplBase::ValidateBlockRoomKey(Block& curr) const {
   if (curr.RoomKey_.GetCurrentRoomType() != this->OpenArgs_.RoomType_)
      Raise<InnFileError>(std::errc::bad_message, "InnStream:Bad RoomType.");
   if (curr.RoomKey_.GetDataSize() < sizeof(RoomHeader))
      Raise<InnRoomSizeError>("InnStream:Bad RoomSize.");
}
size_t InnStream::ImplBase::SeekNoFlush(PosType pos, InnRoomSize& ofsBlock) {
   if (this->LastSeekIndex_ < this->BlockList_.size()) {
      if (this->BlockList_[this->LastSeekIndex_].IsInThisBlock(pos, &ofsBlock))
         return this->LastSeekIndex_ + 1;
   }
   size_t idx = 0;
   for (Block& blk : this->BlockList_) {
      if (blk.IsInThisBlock(pos, &ofsBlock))
         return (this->LastSeekIndex_ = idx) + 1;
      ++idx;
   }
   Block curr;
   if (this->NextRoomPos_)
      curr = this->LoadNextBlock();
   else {
      if (this->OpenArgs_.RoomType_ != kInnRoomType_ExFileHeader || !this->BlockList_.empty())
         return 0;
      curr = this->LoadNextBlock();
      if (!curr.RoomKey_) // InnFile 裡面沒有任何 rooms: 新建立的 InnFile;
         return 0;
   }
   for (;;) {
      this->ValidateBlockRoomKey(curr);
      Block* last = this->BlockList_.empty() ? nullptr : &this->BlockList_.back();
      curr.SPos_ = (last ? (last->SPos_ + last->GetPayloadSize()) : 0u);
      this->BlockList_.emplace_back(std::move(curr));
      last = &this->BlockList_.back();
      if (last->IsInThisBlock(pos, &ofsBlock))
         return (this->LastSeekIndex_ = idx) + 1;
      if (this->NextRoomPos_ == 0)
         return 0;
      ++idx;
      curr = this->LoadNextBlock();
   }
}
InnStream::PosType InnStream::ImplBase::SeekToEndNoFlush() {
   InnRoomSize ofsBlock;
   this->SeekNoFlush(std::numeric_limits<PosType>::max(), ofsBlock);
   if (this->BlockList_.empty())
      return 0;
   this->LastSeekIndex_ = this->BlockList_.size() - 1;
   Block& last = this->BlockList_.back();
   return last.SPos_ + last.GetPayloadSize();
}
//--------------------------------------------------------------------------//
struct InnStream::IoHandler {
   virtual ~IoHandler() {
   }
   SizeType Work(const Locker& impl, PosType pos, void* buf, SizeType bufsz) {
      InnRoomSize ofsBlock;
      size_t      idx = InnStream::SeekWithFlush(impl, pos, ofsBlock);
      if (idx)
         --idx;
      else {
         if (!this->AddNewRoom(impl))
            return 0;
         ofsBlock = 0;
      }
      SizeType totsz = 0;
      for (;;) {
         Block&      curr = impl->BlockList_[idx];
         InnRoomSize iosz = curr.GetBlockSize() - ofsBlock;
         if (iosz > 0) {
            if (iosz > bufsz)
               iosz = static_cast<InnRoomSize>(bufsz);
            iosz = this->WorkRW(impl, curr.RoomKey_, static_cast<InnRoomSize>(ofsBlock + sizeof(RoomHeader)), buf, iosz);
            assert(bufsz >= iosz);
            totsz += iosz;
         }
         if ((bufsz -= iosz) == 0)
            return totsz;
         buf = reinterpret_cast<byte*>(buf) + iosz;
         if (++idx >= impl->BlockList_.size()) {
            if (impl->NextRoomPos_ == 0) {
               if (!this->AddNewRoom(impl))
                  return totsz;
            }
            else {
               Block next = impl->LoadNextBlock();
               impl->ValidateBlockRoomKey(next);
               next.SPos_ = curr.SPos_ + curr.GetBlockSize();
               impl->BlockList_.emplace_back(std::move(next));
            }
         }
         impl->LastSeekIndex_ = idx;
         ofsBlock = 0;
      }
   }
   virtual bool AddNewRoom(const Locker& impl) = 0;
   virtual InnRoomSize WorkRW(const Locker& impl, InnFile::RoomKey& roomKey, InnRoomSize offset, void* buf, InnRoomSize size) = 0;
};
//--------------------------------------------------------------------------//
struct InnStream::IoHandlerRd : public IoHandler {
   InnRoomSize WorkRW(const Locker& impl, InnFile::RoomKey& roomKey, InnRoomSize offset, void* buf, InnRoomSize size) override {
      return impl->OwnerInnFile()->Read(roomKey, offset, buf, size);
   }
   bool AddNewRoom(const Locker& impl) override {
      (void)impl;
      return false;
   }
};
InnStream::SizeType InnStream::Read(PosType pos, void* buf, SizeType bufsz) {
   return IoHandlerRd{}.Work(this->Lock(), pos, buf, bufsz);
}
//--------------------------------------------------------------------------//
struct InnStream::IoHandlerWr : public IoHandler {
   SizeType Write(const Locker& impl, PosType pos, const void* buf, SizeType bufsz) {
      return this->Work(impl, pos, const_cast<void*>(buf), bufsz);
   }
   InnRoomSize WorkRW(const Locker& impl, InnFile::RoomKey& roomKey, InnRoomSize offset, void* buf, InnRoomSize size) {
      return impl->OwnerInnFile()->Write(roomKey, offset, buf, size);
   }
   bool AddNewRoom(const Locker& impl) override {
      assert(impl->NextRoomPos_ == 0);
      size_t ridx = impl->BlockList_.size();
      if (ridx >= numofele(impl->OpenArgs_.ExpectedRoomSize_))
         ridx = numofele(impl->OpenArgs_.ExpectedRoomSize_) - 1;
      InnFileP* ownerInnFile = impl->OwnerInnFile();
      impl->BlockList_.emplace_back(
         ownerInnFile->MakeNewRoom(impl->OpenArgs_.RoomType_,
                                   impl->OpenArgs_.ExpectedRoomSize_[ridx]));
      Block&     newBlock = impl->BlockList_.back();
      RoomHeader roomHeader;
      ZeroStruct(roomHeader);
      ownerInnFile->Write(newBlock.RoomKey_, 0, &roomHeader, sizeof(roomHeader));
      size_t blockCount = impl->BlockList_.size();
      if (blockCount <= 1) {
         newBlock.SPos_ = 0;
         impl->Owner().OnFirstRoomPosChanged(impl, newBlock.RoomKey_.GetRoomPos());
      }
      else {
         Block& prvBlock = impl->BlockList_[blockCount - 2];
         newBlock.SPos_ = prvBlock.SPos_ + prvBlock.GetPayloadSize();
         static_assert(sizeof(roomHeader) == sizeof(roomHeader.NextRoomPos_), "如果 roomHeader 有額外訊息, 要在這裡填妥.");
         PutBigEndian(&roomHeader.NextRoomPos_, newBlock.RoomKey_.GetRoomPos());
         ownerInnFile->Write(prvBlock.RoomKey_, 0, &roomHeader, sizeof(roomHeader));
      }
      return true;
   }
};
InnStream::SizeType InnStream::Write(PosType pos, const void* buf, SizeType bufsz) {
   return IoHandlerWr{}.Write(this->Lock(), pos, buf, bufsz);
}
InnStream::SizeType InnStream::WriteDcQueue(const Locker& impl, PosType pos, DcQueue&& buf) {
   SizeType totsz = 0;
   while (!buf.empty()) {
      auto blk = buf.PeekCurrBlock();
      totsz += IoHandlerWr{}.Write(impl, pos + totsz, blk.first, blk.second);
      buf.PopConsumed(blk.second);
   }
   return totsz;
}

InnStream::SizeType InnStream::Append(const void* buf, SizeType bufsz) {
   Locker  impl{this->Lock()};
   PosType pos = this->SeekToEndWithFlush(impl);
   return pos + IoHandlerWr{}.Write(impl, pos, buf, bufsz);
}
InnStream::SizeType InnStream::Append(DcQueue&& buf) {
   Locker  impl{this->Lock()};
   PosType pos = this->SeekToEndWithFlush(impl);
   return pos + this->WriteDcQueue(impl, pos, std::move(buf));
}

} // namespace fon9
