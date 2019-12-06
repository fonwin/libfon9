/// \file fon9/InnStream.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_InnStream_hpp__
#define __fon9_InnStream_hpp__
#include "fon9/InnFile.hpp"
#include "fon9/MustLock.hpp"
#include "fon9/Endian.hpp"
#include <deque>

namespace fon9 {

/// 針對 MakeNewRoom(); 進行 thread safe 保護.
class fon9_API InnFileP : protected InnFile {
   fon9_NON_COPYABLE(InnFileP);
   using base = InnFile;
   using Locker = std::lock_guard<std::mutex>;
   std::mutex  Mutex_;
public:
   InnFileP() = default;

   using base::Open;
   using base::Write;
   using base::Read;
   using base::MakeRoomKey;

   RoomKey MakeNewRoom(InnRoomType roomType, SizeT size) {
      Locker locker{this->Mutex_};
      return base::MakeNewRoom(roomType, size);
   }
};

/// 使用 InnFile Room 機制的 Stream.
/// - 使用 kInnRoomType_HasNextRoomPos_Flag
/// - RoomExHeader 由 InnStream 處理, 使用者無須理會.
/// - 同一個 Room 串列「不支援」多個 Stream 操作.
/// - 讀寫失敗, 拋出異常.
///   - 例如: 不正確的 Room 格式. 不正確的 RoomPos...
class fon9_API InnStream {
   fon9_NON_COPYABLE(InnStream);
public:
   InnStream() = default;
   InnStream(InnStream&& src) {
      this->operator=(std::move(src));
   }
   InnStream& operator=(InnStream&& src);

   virtual ~InnStream();
   void Close() {
      this->Close(this->Lock());
   }

   using RoomPos = InnFile::RoomPosT;
   using SizeType = File::SizeType;
   using PosType = RoomPos;
   struct OpenArgs {
      InnRoomType RoomType_;
      char        Padding____[3];
      /// - 當預期資料量很大時, 可使用較大的 room size, 效率較高, 但可能會浪費較多空間.
      /// - 當預期資料量不大, 可使用較小的 room size, 但如果實際資料量很大, 則效率不高.
      /// - 依照分配的順序, 依序使用底下的 room size 設定.
      /// - 若 rooms 超過底下的數量, 則之後的 room size 使用最後非 0 的設定.
      InnRoomSize ExpectedRoomSize_[5];
      OpenArgs(InnRoomType roomType) {
         memset(this, 0, sizeof(*this));
         this->RoomType_ = roomType;
      }
   };
   void Open(InnFileP& owner, RoomPos startAt, const OpenArgs& openArgs);

   RoomPos GetFirstRoomPos() const;
   InnFileP* GetOwnerInnFile() const {
      return this->OwnerInnFile_;
   }

   SizeType Read(PosType pos, void* buf, SizeType bufsz);
   SizeType Write(PosType pos, const void* buf, SizeType bufsz);
   SizeType Write(PosType pos, DcQueue&& buf) {
      return this->WriteDcQueue(this->Lock(), pos, std::move(buf));
   }

   /// 返回: 寫入後的 stream size = 寫入的位置 + bufsz;
   PosType Append(const void* buf, SizeType bufsz);
   PosType Append(DcQueue&& buf);

protected:
   struct RoomHeader {
      char  NextRoomPos_[sizeof(RoomPos)];
   };
   struct Block {
      fon9_NON_COPYABLE(Block);
      Block() = default;
      Block(Block&& src) = default;
      Block& operator=(Block&& src) = default;
      Block(InnFile::RoomKey&& src) : RoomKey_{std::move(src)} {
      }

      InnFile::RoomKey  RoomKey_;

      /// 假設某資料佔用了 3 rooms:
      /// - room.1 資料量 100 bytes, room.2 資料量 102 bytes, room.3 資料量 30 bytes;
      ///   - room.1 的 SPos_ = 0;
      ///   - room.2 的 SPos_ = 100;
      ///   - room.3 的 SPos_ = 202;
      PosType  SPos_;

      /// 包含已用 GetPayloadSize() 及剩餘可用 GetRemainSize();
      InnRoomSize GetBlockSize() const {
         return static_cast<InnRoomSize>(this->RoomKey_.GetRoomSize() - sizeof(RoomHeader));
      }
      InnRoomSize GetPayloadSize() const {
         return static_cast<InnRoomSize>(this->RoomKey_.GetDataSize() - sizeof(RoomHeader));
      }
      InnRoomSize GetRemainSize() const {
         return this->RoomKey_.GetRemainSize();
      }
      bool IsInThisBlock(PosType pos, InnRoomSize* ofsBlock) const {
         if (pos < this->SPos_)
            return false;
         if ((pos -= this->SPos_) > this->GetPayloadSize())
            return false;
         if (ofsBlock)
            *ofsBlock = static_cast<InnRoomSize>(pos);
         return true;
      }
   };
   using BlockList = std::deque<Block>;
   struct ImplBase {
      OpenArgs    OpenArgs_{InnRoomType{}};
      BlockList   BlockList_;
      size_t      LastSeekIndex_{0};
      RoomPos     NextRoomPos_;

      InnStream& Owner() const {
         return ContainerOf(Impl::StaticCast(*const_cast<ImplBase*>(this)), &InnStream::Impl_);
      }
      InnFileP* OwnerInnFile() const {
         return this->Owner().OwnerInnFile_;
      }
      void Close() {
         this->BlockList_.clear();
         this->Owner().OwnerInnFile_ = nullptr;
      }
      Block LoadNextBlock() {
         RoomHeader  roomHeader;
         auto retval = this->OwnerInnFile()->MakeRoomKey(this->NextRoomPos_, &roomHeader, sizeof(roomHeader));
         this->NextRoomPos_ = (retval.GetDataSize() >= sizeof(roomHeader)
                               ? GetBigEndian<RoomPos>(&roomHeader.NextRoomPos_)
                               : 0);
         return Block{std::move(retval)};
      }
      void ValidateBlockRoomKey(Block& curr) const;

      /// 執行 Seek 之前, 必定會先呼叫 Owner().Flush();
      /// \retval =0 pos 不正確: pos > streams->size();
      /// \retval >0 this->RoomKeyList_[retval - 1];
      size_t SeekNoFlush(PosType pos, InnRoomSize& ofsBlock);
      size_t SeekToEndNoFlush();
   };
   using Impl = MustLock<ImplBase>;
   Impl      Impl_;
   InnFileP* OwnerInnFile_{nullptr};

   struct IoHandler;
   struct IoHandlerRd;
   struct IoHandlerWr;

   using Locker = Impl::Locker;
   static size_t SeekWithFlush(const Locker& impl, PosType pos, InnRoomSize& ofsBlock) {
      impl->Owner().Flush(impl);
      return impl->SeekNoFlush(pos, ofsBlock);
   }
   static size_t SeekToEndWithFlush(const Locker& impl) {
      impl->Owner().Flush(impl);
      return impl->SeekToEndNoFlush();
   }

   Locker Lock() {
      return Locker{this->Impl_};
   }
   void Close(const Locker& impl);
   SizeType WriteDcQueue(const Locker& impl, PosType pos, DcQueue&& buf);

   /// 衍生者若有使用 buffer, 則應在此將 buffer 寫入.
   /// - 預設 do nothing.
   /// - 實作參考:
   /// \code
   ///   PendingRecs pendings{std::move(this->PendingRecs_)};
   ///   if (pendings.empty())
   ///      return;
   ///   將 pendings 寫入.
   /// \endcode
   virtual void Flush(const Locker& impl);
   /// 預設 do nothing.
   virtual void OnFirstRoomPosChanged(const Locker& impl, RoomPos newFirstRoomPos);
};

} // namesoace fon9
#endif//__fon9_InnStream_hpp__
