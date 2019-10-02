// \file f9twf/ExgMapMgr.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMapMgr.hpp"

namespace f9twf {

struct ExgMapMgr::P06Loader : public fon9::seed::FileImpLoader {
   fon9_NON_COPY_NON_MOVE(P06Loader);
   ExgMapMgr&     Owner_;
   ExgMapBrkFcmId MapBrkFcmId_;
   P06Loader(ExgMapMgr& owner, size_t fileSize)
      : Owner_{owner} {
      this->MapBrkFcmId_.reserve(fileSize / sizeof(TmpP06) + 1);
   }
   ~P06Loader() {
      Maps::Locker maps{this->Owner_.Maps_};
      maps->MapBrkFcmId_.swap(this->MapBrkFcmId_);
   }
   size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
      return this->ParseBlock(pbuf, bufsz, isEOF, sizeof(TmpP06));
   }
   void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
      (void)bufsz; (void)isEOF;
      assert(bufsz == sizeof(TmpP06));
      this->MapBrkFcmId_.AddItem(*reinterpret_cast<TmpP06*>(pbuf));
   }
};

struct ExgMapMgr::P07Loader : public fon9::seed::FileImpLoader {
   fon9_NON_COPY_NON_MOVE(P07Loader);
   ExgMapMgr&        Owner_;
   ExgMapSessionDn   MapSessionDn_;
   ExgMapBrkFcmId*   MapBrkFcmId_;
   P07Loader(ExgMapMgr& owner, size_t fileSize)
      : Owner_{owner} {
      this->MapSessionDn_.reserve(fileSize / sizeof("f906000.session1.tmp.fut.taifex,30002"));
   }
   ~P07Loader() {
      Maps::Locker maps{this->Owner_.Maps_};
      maps->MapSessionDn_.swap(this->MapSessionDn_);
   }
   size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF) override {
      Maps::Locker maps{this->Owner_.Maps_};
      this->MapBrkFcmId_ = &maps->MapBrkFcmId_;
      return this->ParseLine(pbuf, bufsz, isEOF);
   }
   void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF) override {
      (void)bufsz; (void)isEOF;
      this->MapSessionDn_.FeedLine(*this->MapBrkFcmId_, fon9::StrView{pbuf, bufsz});
   }
};

template <class Loader>
class ExgMapMgr_ImpSeed : public fon9::seed::FileImpSeed {
   fon9_NON_COPY_NON_MOVE(ExgMapMgr_ImpSeed);
   using base = fon9::seed::FileImpSeed;

public:
   ExgMapMgr& ExgMapMgr_;

   template <class... ArgsT>
   ExgMapMgr_ImpSeed(ExgMapMgr& mgr, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , ExgMapMgr_(mgr) {
   }

   fon9::seed::FileImpLoaderSP OnBeforeLoad(uint64_t fileSize) override {
      return new Loader(this->ExgMapMgr_, fileSize);
   }
   void OnAfterLoad(fon9::RevBuffer& rbuf, fon9::seed::FileImpLoaderSP loader) override {
      (void)rbuf; (void)loader;
   }
};

fon9::seed::FileImpTreeSP ExgMapMgr::MakeSapling(ExgMapMgr& rthis) {
   if (0);//TODO: 檔案有異動時, 自動重新載入?
   // P07/PA7: FcmId+SessionId 是唯一?
   // - 不論 SystemType: 10選擇權日盤、11選擇權夜盤、20期貨日盤、21期貨夜盤?
   //   - 如果不同 SystemType 可能有相同的 SessionId, 則要有 4 個 MapSessionDn_; 或是 seskey 要加上 SystemType;
   // - 檔名: "P07.SystemType.FXXX", 所以要有 4 個 P07.xx 的 Loader?
   fon9::seed::FileImpTreeSP retval{new fon9::seed::FileImpTree{"ExgImps"}};
   retval->Add(new ExgMapMgr_ImpSeed<P06Loader>(rthis, *retval, "P06", "./tmpsave/P06"));
   retval->Add(new ExgMapMgr_ImpSeed<P07Loader>(rthis, *retval, "P07", "./tmpsave/P07"));
   return retval;
}

} // namespaces
