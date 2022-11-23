/// \file fon9/fmkt/TradingLineManagerSeed.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/TradingLineManagerSeed.hpp"
#include "fon9/fmkt/TradingLine.hpp"

namespace fon9 { namespace fmkt {

TradingLineManagerSeed::~TradingLineManagerSeed() {
}
void TradingLineManagerSeed::OnSeedCommand(seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler,
                                           seed::MaTreeBase::Locker&& ulk, seed::SeedVisitor* visitor) {
   if (cmdln == "?") {
      res.OpResult_ = seed::OpResult::no_error;
      resHandler(res,
       //"xx" fon9_kCSTR_CELLSPL "需要輸入參數的指令" fon9_kCSTR_CELLSPL "輸入參數的提示字串." fon9_kCSTR_ROWSPL
         "st" fon9_kCSTR_CELLSPL "State info"
      );
      return;
   }
   if (iequals(cmdln, "st")) {
      if (auto* lmgr = dynamic_cast<TradingLineManager*>(this->Sapling_.get())) {
         fon9::RevBufferList rbuf{256};
         lmgr->Lock()->GetStInfo(rbuf);
         resHandler(res, ToStrView(BufferTo<std::string>(rbuf.MoveOut())));
         return;
      }
   }
   base::OnSeedCommand(res, cmdln, std::move(resHandler), std::move(ulk), visitor);
}

} } // namespaces
