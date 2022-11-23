/// \file fon9/fmkt/TradingLineManagerSeed.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_TradingLineManagerSeed_hpp__
#define __fon9_fmkt_TradingLineManagerSeed_hpp__
#include "fon9/seed/MaTree.hpp"

namespace fon9 { namespace fmkt {

class fon9_API TradingLineManagerSeed : public seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(TradingLineManagerSeed);
   using base = seed::NamedSapling;
public:
   using base::base;
   ~TradingLineManagerSeed();
   /// 提供現在的 [線路 & Helper] 狀態
   void OnSeedCommand(seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler,
                      seed::MaTreeBase::Locker&& ulk, seed::SeedVisitor* visitor) override;
};


} } // namespaces
#endif//__fon9_fmkt_TradingLineManagerSeed_hpp__
