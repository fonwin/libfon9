// \file fon9/InnApf.cpp
// \author fonwinz@gmail.com
#include "fon9/InnApf.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/Log.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace fon9 {

using ApfVerT = uint8_t;
constexpr ApfVerT kApfVer0 = 0;
constexpr byte    kSizeExHeader1st = static_cast<byte>(
   sizeof(ApfVerT)            // ApfVer;
   + sizeof(kSizeExHeader1st)
   + sizeof(InnFile::RoomPosT)// FreeRoomPos1st_; // 第1個空房的位置.
   + (32 - 10));              /* 額外保留空間 */
//--------------------------------------------------------------------------//
constexpr size_t  kInnApfSavedStreamInfoSize = sizeof(InnApf::StreamInfo);

static inline size_t ToBitv(RevBuffer& rbuf, const InnApf::StreamInfo& info) {
   static_assert(sizeof(info) == sizeof(info.FirstRoomPos_), "有其他欄位需要儲存&載入.");
   return ToBitv(rbuf, info.FirstRoomPos_);
}
static inline void BitvTo(DcQueue& buf, InnApf::StreamInfo& info) {
   return BitvTo(buf, info.FirstRoomPos_);
}

template <class Archive>
void Serialize(Archive& ar, ArchiveWorker<Archive, InnApf::StreamRec>& rec) {
   return ar(rec.Key_, rec.Info_);
}
//--------------------------------------------------------------------------//
void InnApf::Stream::UpdateChanged(InnStream& exHeader) {
   RevBufferList  rbuf{128};
   auto impl = this->Lock();
   this->Flush(impl);
   if (this->OffsetExHeader_ == 0) {
      this->Saved_.Info_ = this->CurrInfo_;
      BitvOutArchive oar{rbuf};
      oar(this->Saved_);
      fon9::ByteArraySizeToBitvT(rbuf, fon9::CalcDataSize(rbuf.cfront()));
      this->OffsetExHeader_ = exHeader.Append(DcQueueList{rbuf.MoveOut()})
                            - kInnApfSavedStreamInfoSize;
   }
   else if (this->Saved_.Info_ != this->CurrInfo_) {
      this->Saved_.Info_ = this->CurrInfo_;
      impl.unlock();
      ToBitv(rbuf, this->Saved_.Info_);
      exHeader.Write(this->OffsetExHeader_, DcQueueList{rbuf.MoveOut()});
   }
}
void InnApf::UpdateChanged() {
   StreamList  changedStreams{std::move(KeyMap::Locker{this->KeyMap_}->ChangedStreams_)};
   for (Stream* stream : changedStreams)
      stream->UpdateChanged(this->ExHeader_);
}
void InnApf::EmitOnUpdateTimer(TimerEntry* timer, TimeStamp now) {
   (void)now;
   InnApf& rthis = ContainerOf(*static_cast<decltype(InnApf::UpdateTimer_)*>(timer), &InnApf::UpdateTimer_);
   if(KeyMap::Locker{rthis.KeyMap_}->ChangedStreams_.empty()) {
      rthis.StartUpdateTimer();
      return;
   }
   // 為了避免佔用 timer thread 的時間, 所以切到 DefaultThreadPool 處理.
   InnApfSP pthis{&rthis};
   GetDefaultThreadPool().EmplaceMessage([pthis]() {
      pthis->UpdateChanged();
      pthis->StartUpdateTimer();
   });
}
//--------------------------------------------------------------------------//
InnApfSP InnApf::Make(OpenArgs& args, const InnStream::OpenArgs& streamOpenArgs, OpenResult& res) {
   InnApfSP retval{new InnApf{streamOpenArgs}};
   try {
      res = retval->InnFile_.Open(args);
      if (res.IsError())
         return nullptr;
      InnStream::OpenArgs sOpenArgs{kInnRoomType_ExFileHeader};
      sOpenArgs.ExpectedRoomSize_[0] = 1000 * 10 * 20; // 1萬個keys, 每個 key 占用 20 bytes(包含 text + FirstRoomPos);
      retval->ExHeader_.Open(retval->InnFile_, 0u, sOpenArgs);
      if (res.GetResult() == 0)
         retval->OnNewInnFile();
      else {
         if (const char* errmsg = retval->ReadExHeader()) {
            fon9_LOG_ERROR("InnApf.ReadExHeader|fileName=", args.FileName_, "|err=", errmsg);
            res = std::errc::bad_message;
            return nullptr;
         }
         res = OpenResult{KeyMap::Locker{retval->KeyMap_}->size()};
      }
      retval->StartUpdateTimer();
      return retval;
   }
   catch (std::exception& e) {
      fon9_LOG_ERROR("InnApf.Open|fileName=", args.FileName_, "|err=", e.what());
      res = std::errc::bad_message;
      return nullptr;
   }
}
void InnApf::OnNewInnFile() {
   byte buf[kSizeExHeader1st];
   memset(buf, 0, sizeof(buf));
   PutBigEndian(buf, kApfVer0);
   PutBigEndian(buf + sizeof(kApfVer0), kSizeExHeader1st);
   this->ExHeader_.Write(0, buf, kSizeExHeader1st);
}
const char* InnApf::ReadExHeader() {
   char  buf[1024 * 64];
   auto  rdsz = this->ExHeader_.Read(0, buf, sizeof(buf));
   if (rdsz < kSizeExHeader1st)
      return "Bad ExHeader size";
   if (GetBigEndian<ApfVerT>(buf) != kApfVer0)
      return "Unknown ApfVer";
   if (GetBigEndian<decltype(kSizeExHeader1st)>(buf + sizeof(ApfVerT)) != kSizeExHeader1st)
      return "Unknown Apf.ExHeaderSize";
   this->FreeRoomPos1st_ = GetBigEndian<RoomPos>(buf + sizeof(ApfVerT) + sizeof(kSizeExHeader1st));

   DcQueueFixedMem dcq{buf + kSizeExHeader1st, rdsz - kSizeExHeader1st};
   SPosT           ofsExHdr = kSizeExHeader1st;
   RoomPos         posExHdr = rdsz;
   size_t          szRec = 0;
   StreamRec       rec;
   KeyMap::Locker  map{this->KeyMap_};
   for (;;) {
      for (;;) {
         auto hdrSize = PeekBitvByteArraySize(dcq, szRec);
         if (fon9_UNLIKELY(hdrSize < PeekBitvByteArraySizeR::DataReady))
            break;
         ofsExHdr += static_cast<SPosT>(hdrSize) + szRec;
         dcq.PopConsumed(static_cast<size_t>(hdrSize));
         BitvInArchive iar{dcq};
         iar(rec);
         map->AddStream(std::move(rec), ofsExHdr - kInnApfSavedStreamInfoSize);
      }
      auto remainSize = dcq.GetCurrBlockSize();
      if (remainSize >= sizeof(buf))
         return "Unknown format";
      memcpy(buf, dcq.Peek1(), remainSize);
      rdsz = this->ExHeader_.Read(posExHdr, buf + remainSize, sizeof(buf) - remainSize);
      if (rdsz <= 0) {
         if (remainSize > 0)
            return "Unknown format2";
         return nullptr;
      }
      dcq.Reset(buf, buf + remainSize + rdsz);
      posExHdr += rdsz;
   }
   return nullptr;
}
//--------------------------------------------------------------------------//
InnApf::Stream::Stream(InnApf& owner, StrView key) {
   this->Saved_.Key_.assign(key);
   ZeroStruct(this->Saved_.Info_);
   this->Ctor(owner);
}
InnApf::Stream::Stream(InnApf& owner, StreamRec&& rec, SPosT offsetExHeader)
   : OffsetExHeader_{offsetExHeader}
   , Saved_{std::move(rec)} {
   this->Ctor(owner);
}
void InnApf::Stream::Ctor(InnApf& owner) {
   this->CurrInfo_ = this->Saved_.Info_;
   this->Open(owner.InnFile_, this->Saved_.Info_.FirstRoomPos_.Value_, owner.StreamOpenArgs_);
}
void InnApf::Stream::Flush(const Locker& impl) {
   if (this->IsFlushing_)
      return;
   PendingBufferImpl pendings{std::move(*PendingBuffer::Locker{this->PendingAppend_})};
   if (pendings.empty())
      return;
   struct AutoSetFlag {
      Stream* Owner_;
      AutoSetFlag(Stream* owner) : Owner_(owner) {
         owner->IsFlushing_ = true;
      }
      ~AutoSetFlag() {
         this->Owner_->IsFlushing_ = false;
      }
   };
   AutoSetFlag autoSetFlag{this};
   this->WriteDcQueue(impl, impl->SeekToEndNoFlush(), std::move(pendings));
   this->CheckCurrInfoChanged();
}
void InnApf::Stream::AppendBuffered(BufferList&& buf) {
   if (buf.empty())
      return;
   else {
      PendingBuffer::Locker pendings{this->PendingAppend_};
      this->IsNeedsChangedUpdate_ = pendings->empty();
      pendings->push_back(std::move(buf));
   }  // auto unlock.
   this->CheckCurrInfoChanged();
}
void InnApf::Stream::AppendBuffered(const void* pbufmem, SizeT bufsz, SizeT sizeReserved) {
   if (bufsz <= 0)
      return;
   else {
      PendingBuffer::Locker pendings{this->PendingAppend_};
      this->IsNeedsChangedUpdate_ = pendings->empty();
      pendings->Append(pbufmem, bufsz, sizeReserved);
   }  // auto unlock.
   this->CheckCurrInfoChanged();
}
void InnApf::Stream::CheckCurrInfoChanged() {
   if (this->IsNeedsChangedUpdate_) {
      this->IsNeedsChangedUpdate_ = false;
      this->Owner().KeyMap_.Lock()->ChangedStreams_.emplace_back(this);
   }
}
void InnApf::Stream::OnFirstRoomPosChanged(const Locker& impl, RoomPos newFirstRoomPos) {
   (void)impl;
   this->IsNeedsChangedUpdate_ = true;
   this->CurrInfo_.FirstRoomPos_.Value_ = newFirstRoomPos;
}
InnApf::Stream* InnApf::KeyMapImpl::AddStream(StreamRec&& rec, SPosT ofsExHeader) {
   StreamSP stream{new Stream{this->Owner(), std::move(rec), ofsExHeader}};
   auto     ins = this->emplace(stream->Key(), stream);
   assert(ins.second && ins.first->second == stream); (void)ins;
   return stream.get();
}
//--------------------------------------------------------------------------//
InnApf::OpenResult InnApf::KeyMapImpl::OpenStream(StrView key, FileMode fm, StreamSP& out) {
   InnApf& owner = this->Owner();
   if (IsEnumContainsAny(fm, FileMode::OpenAlways | FileMode::CreatePath)) {
      StreamSP& ref = (*this)[CharVector::MakeRef(key)];
      if (ref.get() == nullptr) {
         ref.reset(new Stream{owner, key});
         this->ChangedStreams_.push_back(ref.get());
      }
      else if (IsEnumContains(fm, FileMode::MustNew))
         return OpenResult{std::errc::file_exists};
      out = ref;
   }
   else {
      auto ifind = this->find(CharVector::MakeRef(key));
      if (ifind == this->end())
         return OpenResult{std::errc::no_such_file_or_directory};
      if (IsEnumContains(fm, FileMode::MustNew))
         return OpenResult{std::errc::file_exists};
      assert(ifind->second.get() != nullptr);
      out = ifind->second;
   }
   intrusive_ptr_add_ref(&owner); // 在 InnApf::StreamRW::Close() 釋放;
   return OpenResult(0);
}
InnApf::OpenResult InnApf::StreamRW::Open(InnApf& src, StrView key, FileMode fm) {
   this->Close();
   this->OpenMode_ = fm;
   return KeyMap::Locker{src.KeyMap_}->OpenStream(key, fm, this->Stream_);
}
void InnApf::StreamRW::Close() {
   if (StreamSP stream = std::move(this->Stream_))
      intrusive_ptr_release(&stream->Owner()); // add_ref 在 InnApf::KeyMapImpl::OpenStream();
}

} // namespace fon9
