/// \file fon9/InnApf.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_InnApf_hpp__
#define __fon9_InnApf_hpp__
#include "fon9/InnStream.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/BitvFixedInt.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/Timer.hpp"
#include <unordered_map>

namespace fon9 {

class InnApf;
using InnApfSP = intrusive_ptr<InnApf>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup Inn
/// 在「一個檔案」裡面儲存 n 個可持續由 key(最長 255 bytes) 索引的資料.
/// - 儲存的資料可用類似檔案讀寫的方式操作:
///   - Read、Wite、Append、Reduce;
///   - 僅適合循序讀寫, 及 Append.
/// - 使用方式:
///   \code
///      StreamRW srw;
///      srw.Open(innApf, key, fileMode);
///      srw.Read(pos, ...);
///      srw.Write(pos, ...);
///      srw.AppendBuffered();
///      srw.AppendNoBuffer();
///   \endcode
/// - 使用 InnFile 實作.
/// - InnFile.FileExHeader: 儲存 key 及其相關的索引.
///   - RoomPos  NextExHeaderRoomPos;
///   - uint8_t  ApfVer;
///   - uint8_t  ExHeaderSize;   // 當 ApfVer==0; 此值為 32;
///   - RoomPos  FreeRoomPos1st; // 第1個空房的位置.
///   - StreamRec[];             // Bitv format.
///   - 後續的 ExHeader room 只有 NextExHeaderRoomPos 及 StreamRec[];
class fon9_API InnApf : public intrusive_ref_counter<InnApf> {
   fon9_NON_COPY_NON_MOVE(InnApf);
   InnApf() = delete;

public:
   /// size for read/write;
   using SizeT = size_t;
   /// position for stream read/write;
   using SPosT = uint64_t;
   /// key for stream's name;
   using SKeyT = CharVector;

   using RoomPos = InnFile::RoomPosT;
   using InnBitvFixedRoomPos = BitvFixedUInt<sizeof(RoomPos) - 1>;

public: // 雖然為 public, 但一般不會直接使用到, 只是為了在 InnApf.cpp 裡面可以直接取用.
   /// 與 key 一同記錄在 FileExHeader 的資訊.
   /// InnFile 開啟後, 重建 stream 時使用.
   struct StreamInfo {
      InnBitvFixedRoomPos  FirstRoomPos_;
      bool operator==(const StreamInfo& rhs) const {
         return this->FirstRoomPos_.Value_ == rhs.FirstRoomPos_.Value_;
      }
      bool operator!=(const StreamInfo& rhs) const {
         return !this->operator==(rhs);
      }
   };
   struct StreamRec {
      SKeyT       Key_;
      StreamInfo  Info_;
   };
   using RoomKey = InnFile::RoomKey;
   class Stream : public intrusive_ref_counter<Stream>, private InnStream {
      fon9_NON_COPY_NON_MOVE(Stream);
      using base = InnStream;
      bool IsNeedsChangedUpdate_{false};
      bool IsFlushing_{false};
      /// 需要寫入 FileExHeader 的內容.
      /// - CurrInfo_ != Saved_.Info_; 才需要寫入.
      /// - 寫入位置為 OffsetExHeader_;
      /// - 若 OffsetExHeader_ == 0 表示尚未寫入 FileExHeader;
      StreamInfo  CurrInfo_;
      /// 可在此 offset 更新 Saved_.Info_; (不含 Key 的 StreamInfo);
      SPosT       OffsetExHeader_{0};
      StreamRec   Saved_;

      using PendingBufferImpl = DcQueueList;
      using PendingBuffer = MustLock<PendingBufferImpl>;
      PendingBuffer  PendingAppend_;

      void Ctor(InnApf& owner);
      void Flush(const Locker& impl) override;
      void OnFirstRoomPosChanged(const Locker& impl, RoomPos newFirstRoomPos) override;
   public:
      Stream(InnApf& owner, StreamRec&& rec, SPosT offsetExHeader);
      Stream(InnApf& owner, StrView key);
      ~Stream() {
         this->Flush(this->Lock());
      }

      InnApf& Owner() const {
         return ContainerOf(*this->GetOwnerInnFile(), &InnApf::InnFile_);
      }
      const SKeyT& Key() const {
         return this->Saved_.Key_;
      }
      using base::Size;

      SizeT Read(SPosT pos, void* buf, SizeT bufsz) {
         return base::Read(pos, buf, bufsz);
      }
      SizeT Write(SPosT pos, const void* buf, SizeT bufsz) {
         auto retval = base::Write(pos, buf, bufsz);
         this->CheckCurrInfoChanged();
         return retval;
      }
      SizeT Write(SPosT pos, DcQueue&& buf) {
         auto retval = base::Write(pos, std::move(buf));
         this->CheckCurrInfoChanged();
         return retval;
      }
      SPosT AppendNoBuffer(const void* pbufmem, SizeT bufsz) {
         auto retval = base::Append(pbufmem, bufsz);
         this->CheckCurrInfoChanged();
         return retval;
      }
      void AppendBuffered(BufferList&& buf);
      void AppendBuffered(const void* pbufmem, SizeT bufsz, SizeT sizeReserved);
      void CheckCurrInfoChanged();
      void UpdateChanged(InnStream& exHeader);
   };
   using StreamSP = intrusive_ptr<Stream>;

public:
   const InnStream::OpenArgs  StreamOpenArgs_;
   InnApf(const InnStream::OpenArgs& streamOpenArgs)
      : StreamOpenArgs_{streamOpenArgs} {
   }

   ~InnApf() {
      this->UpdateTimer_.DisposeAndWait();
      this->UpdateChanged();
   }
   using OpenArgs = InnFile::OpenArgs;
   using OpenResult = InnFile::OpenResult;
   /// - 若傳回 nullptr, 則透過 res 取得錯誤原因.
   /// - ores.GetResult() == key 的數量.
   static InnApfSP Make(OpenArgs& args, const InnStream::OpenArgs& streamOpenArgs, OpenResult& res);

   class fon9_API StreamRW {
      fon9_NON_COPYABLE(StreamRW);
      StreamSP Stream_;
      FileMode OpenMode_;
   public:
      StreamRW() = default;
      StreamRW(StreamRW&& src) = default;
      StreamRW& operator=(StreamRW&& src) = default;
      ~StreamRW() {
         this->Close();
      }
      bool IsReady() const {
         return this->Stream_.get() != nullptr;
      }
      StreamSP GetStream() const {
         return this->Stream_;
      }
      StrView GetKey() const {
         return this->Stream_ ? ToStrView(this->Stream_->Key()) : nullptr;
      }
      InnApf* Owner() const {
         return this->Stream_ ? &this->Stream_->Owner() : nullptr;
      }
      SizeT Size() const {
         return this->Stream_ ? this->Stream_->Size() : 0;
      }
      /// - 若開啟失敗, 則返回後 this->IsReady() == false;
      /// - 若開啟成功: retval.GetResult() == 0;
      OpenResult Open(InnApf& src, StrView key, FileMode fm);
      void Close();

      /// 從指定位置讀出.
      /// \return 讀出的資料量, 必定 <= bufsz; 
      SizeT Read(SPosT pos, void* buf, SizeT bufsz) {
         assert(this->IsReady());
         return this->Stream_->Read(pos, buf, bufsz);
      }
      /// 從指定位置寫入.
      /// \return 寫入的資料量, 必定 == bufsz; 
      SizeT Write(SPosT pos, const void* buf, SizeT bufsz) {
         assert(this->IsReady());
         return this->Stream_->Write(pos, buf, bufsz);
      }
      SizeT Write(SPosT pos, DcQueue&& buf) {
         assert(this->IsReady());
         return this->Stream_->Write(pos, std::move(buf));
      }
      /// 在尾端寫入.
      /// 返回: 寫入後的 stream size = 寫入的位置 + bufsz;
      SPosT AppendNoBuffer(const void* pbufmem, SizeT bufsz) {
         assert(this->IsReady());
         return this->Stream_->AppendNoBuffer(pbufmem, bufsz);
      }
      /// 將 buf 放入 queue, 在另一個 thread 執行寫入動作.
      void AppendBuffered(BufferList&& buf) {
         assert(this->IsReady());
         return this->Stream_->AppendBuffered(std::move(buf));
      }
      void AppendBuffered(const void* pbufmem, SizeT bufsz, SizeT sizeReserved = 0) {
         assert(this->IsReady());
         return this->Stream_->AppendBuffered(pbufmem, bufsz, sizeReserved);
      }
   };

private:
   const char* ReadExHeader();
   void OnNewInnFile();
   void UpdateChanged();

   using StreamList = std::deque<Stream*>;
   class KeyMapImpl : protected std::unordered_map<SKeyT, StreamSP> {
      using base = std::unordered_map<SKeyT, StreamSP>;
   public:
      using base::size;

      /// 當 Stream 有異動, 需要寫入 ExHeader 時, 依照加入的順序存放於此.
      StreamList  ChangedStreams_;

      InnApf& Owner() {
         return ContainerOf(KeyMap::StaticCast(*this), &InnApf::KeyMap_);
      }

      Stream* AddStream(StreamRec&& rec, SPosT ofsExHeader);

      /// - 若 fm 包含 FileMode::OpenAlways | FileMode::CreatePath 若 key 不存在, 則會建立.
      /// - 若 fm 包含 FileMode::MustNew 則 key 必須不存在, 若已存在則返回 file_exists;
      /// - 若返回失敗, 則 out 內容不變.
      /// - 若返回成功, 則 intrusive_ptr_add_ref(&owner);
      OpenResult OpenStream(StrView key, FileMode fm, StreamSP& out);
   };
   using KeyMap = MustLock<KeyMapImpl>;
   KeyMap      KeyMap_;
   InnFileP    InnFile_;
   RoomPos     FreeRoomPos1st_; 
   InnStream   ExHeader_;

   void StartUpdateTimer() {
      this->UpdateTimer_.RunAfter(TimeInterval_Second(1));
   }
   static void EmitOnUpdateTimer(TimerEntry* timer, TimeStamp now);
   DataMemberEmitOnTimer<&InnApf::EmitOnUpdateTimer> UpdateTimer_;
};
fon9_WARN_POP;

} // namesoace fon9
#endif//__fon9_InnApf_hpp__
