// \file f9twf/ExgMapMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMapMgr_hpp__
#define __f9twf_ExgMapMgr_hpp__
#include "f9twf/ExgMapSessionDn.hpp"
#include "fon9/seed/FileImpTree.hpp"

namespace f9twf {

/// 期交所提供給期貨商的一些基本資料對照表.
/// - P06: BrkId <-> FcmId 對照.
/// - P07: SocketId -> dn:Port 對照.
class f9twf_API ExgMapMgr : public fon9::seed::FileImpMgr {
   fon9_NON_COPY_NON_MOVE(ExgMapMgr);
   using base = fon9::seed::FileImpMgr;

   struct P06Loader;
   struct P07Loader;
   struct MapsImpl {
      ExgMapBrkFcmId    MapBrkFcmId_;
      ExgMapSessionDn   MapSessionDn_;
   };
   using Maps = fon9::MustLock<MapsImpl>;
   Maps  Maps_;

   static fon9::seed::FileImpTreeSP MakeSapling(ExgMapMgr& rthis);

public:
   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list.
   template <class... ArgsT>
   ExgMapMgr(ArgsT&&... args)
      : base(MakeSapling(*this), std::forward<ArgsT>(args)...) {
   }
   fon9_MSC_WARN_POP;

   BrkId GetBrkId(FcmId id) const {
      Maps::ConstLocker maps{this->Maps_};
      return maps->MapBrkFcmId_.GetBrkId(id);
   }
   FcmId GetFcmId(BrkId id) const {
      Maps::ConstLocker maps{this->Maps_};
      return maps->MapBrkFcmId_.GetFcmId(id);
   }
   bool AppendDn(const ExgLineTmpArgs& lineArgs, std::string& devcfg) const {
      Maps::ConstLocker maps{this->Maps_};
      return maps->MapSessionDn_.AppendDn(lineArgs, devcfg);
   }
};
using ExgMapMgrSP = fon9::intrusive_ptr<ExgMapMgr>;

} // namespaces
#endif//__f9twf_ExgMapMgr_hpp__
