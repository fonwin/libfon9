// \file fon9/seed/FileImpTree.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_seed_FileImpTree_hpp__
#define __fon9_seed_FileImpTree_hpp__
#include "fon9/seed/MaConfigTree.hpp"
#include "fon9/SchTask.hpp"

namespace fon9 { namespace seed {

class fon9_API FileImpTree;

/// 執行載入後解析工作.
/// - 由 Importer 的衍生者複寫 OnBeforeLoad() 返回.
struct fon9_API FileImpLoader : public intrusive_ref_counter<FileImpLoader> {
   unsigned LnCount_{0};

   virtual ~FileImpLoader();

   /// 傳回 pbuf 的剩餘量.
   /// 預設: return this->ParseLine(pbuf, bufsz, isEOF);
   virtual size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF);

   size_t ParseLine(char* pbuf, size_t bufsz, bool isEOF);
   size_t ParseBlock(char* pbuf, size_t bufsz, bool isEOF, size_t lnsz);
   /// 使用 ParseLine() 或 ParseBlock() 時, 每一行到此處理.
   virtual void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF);
};
using FileImpLoaderSP = intrusive_ptr<FileImpLoader>;

fon9_WARN_DISABLE_PADDING;
/// 一筆匯入資料的設定、狀態、載入.
/// - Title = Import file name;
/// - Description = Import result;
/// - SeedCommand: "reload" 可手動載入.
/// - 衍生者須完成:
///   - `FileImpLoaderSP OnBeforeLoad(uint64_t fileSize) override;`
///   - `void OnAfterLoad(RevBuffer& rbuf, FileImpLoaderSP loader) override;`
class fon9_API FileImpSeed : public NamedSeed {
   fon9_NON_COPY_NON_MOVE(FileImpSeed);
   using base = NamedSeed;

   bool IsReloading_{false};

   /// ulk 處在 unlock 狀態, 如果需要更新 this->SetDescription() 之類, 要先 ulk.lock();
   void OnSeedCommand(SeedOpResult& res,
                      StrView cmdln,
                      FnCommandResultHandler resHandler,
                      MaTreeBase::Locker&& ulk) override;

   virtual FileImpLoaderSP OnBeforeLoad(uint64_t fileSize) = 0;
   virtual void OnAfterLoad(RevBuffer& rbuf, FileImpLoaderSP loader) = 0;

public:
   FileImpTree& Owner_;

   template <class... ArgsT>
   FileImpSeed(FileImpTree& owner, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , Owner_(owner) {
   }

   ~FileImpSeed();

   /// 載入完畢(或失敗)後才會返回.
   /// - 返回時 ulk 必定在 lock() 狀態.
   /// - 返回時 this->GetDescription() 顯示結果.
   /// - 如果重複進入, 則後進者會立即返回, this->GetDescription() 顯示 "Loading";
   void LoadFrom(MaTreeBase::Locker&& ulk, StrView fname);
};
using FileImpSeedSP = intrusive_ptr<FileImpSeed>;
fon9_WARN_POP;

class fon9_API FileImpTree : public MaTree {
   fon9_NON_COPY_NON_MOVE(FileImpTree);
   using base = MaTree;
public:
   using base::base;

   /// 全部載入完畢後才返回.
   void LoadAll();
};
using FileImpTreeSP = intrusive_ptr<FileImpTree>;

/// 管理一組匯入資料的設定、狀態、載入.
/// - SeedCommand: "reload" 全部載入.
/// - 排程時間到, 會切換到 DefaultThreadPool 載入, 避免佔用 DefaultTimerThread 的執行權.
/// - 透過 MaConfigSeed::SetConfigValue() 設定排程參數.
class fon9_API FileImpMgr : public MaConfigSeed, SchTask {
   fon9_NON_COPY_NON_MOVE(FileImpMgr);
   using base = MaConfigSeed;

   void OnParentTreeClear(Tree&) override;
   void OnSeedCommand(SeedOpResult& res,
                      StrView cmdln,
                      FnCommandResultHandler resHandler,
                      MaTreeBase::Locker&& ulk) override;

   void OnSchTask_StateChanged(bool isInSch) override;
   void OnSchTask_NextCheckTime(const SchConfig::CheckResult&) override;
   TimeStamp GetNextSchInTime();
   void SetNextTimeInfo(TimeStamp tmNext, StrView exInfo);

   void OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val);

public:
   const FileImpTreeSP Sapling_;

   template <class... ArgsT>
   FileImpMgr(FileImpTreeSP sapling, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , Sapling_{std::move(sapling)} {
   }

   ~FileImpMgr();

   TreeSP GetSapling() override;

   /// 全部載入完畢後才返回.
   void LoadAll();
};

} } // namespaces
#endif//__fon9_seed_FileImpTree_hpp__
