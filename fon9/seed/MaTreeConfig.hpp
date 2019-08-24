/// \file fon9/seed/MaTreeConfig.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_MaTreeConfig_hpp__
#define __fon9_seed_MaTreeConfig_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/ConfigFileBinder.hpp"

namespace fon9 { namespace seed {

fon9_API void RevPrintConfigGridView(RevBuffer& rbuf, const Fields& flds,
                                     NamedSeedContainerImpl::iterator ibeg,
                                     NamedSeedContainerImpl::iterator iend);

class fon9_API MaTreeConfig : public MaTree {
   fon9_NON_COPY_NON_MOVE(MaTreeConfig);
   using base = MaTree;

   fon9::ConfigFileBinder  ConfigFileBinder_;

   void LoadConfigStr(StrView cfgstr);

protected:
   void OnMaTree_AfterUpdated(Locker& container, NamedSeed& item) override;

public:
   using base::base;

   /// 傳回失敗訊息.
   std::string BindConfigFile(std::string cfgfn);
};

class fon9_API MaTreeConfigItem : public NamedSeed {
   fon9_NON_COPY_NON_MOVE(MaTreeConfigItem);
   using base = NamedSeed;
public:
   using base::base;

   static LayoutSP MakeLayout(std::string tabName, TabFlag tabFlag = TabFlag::NoSapling_NoSeedCommand_Writable);

   fon9::CharVector  Value_;
};

} } // namespaces
#endif//__fon9_seed_MaTreeConfig_hpp__
