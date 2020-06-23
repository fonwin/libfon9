// \file f9twf/ExgMdFmtClosingToRts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtClosingToRts.hpp"
#include "f9twf/ExgMdFmtClosing.hpp"
#include "f9twf/ExgMdFmtParsers.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

static void I071ClosingToRts(ExgMcMessage& e, const ExgMdClosing071& pk) {
   f9fmkt::SymbFuoClosing_Data closing;

   ExgMdLocker lk{e, pk.ProdId_};
   auto&       symb = *e.Symb_;
   ExgMdPriceTo(closing.PriSettlement_, pk.PriSettlement_, symb.PriceOrigDiv_);
   if (closing.PriSettlement_.IsZero())
      closing.PriSettlement_.AssignNull();
   if (symb.FuoClosing_.Data_ == closing)
      return;
   symb.FuoClosing_.Data_ = closing;
   fon9::RevBufferList rbuf{128};
   auto* tabClosing = lk.Symbs_.LayoutSP_->GetTab(fon9_kCSTR_TabName_Closing);
   f9fmkt::MdRtsPackTabValues(rbuf, *tabClosing, symb.FuoClosing_);
   symb.MdRtStream_.Publish(ToStrView(symb.SymbId_),
                            f9sv_RtsPackType_TabValues_AndInfoTime,
                            e.Pk_.InformationTime_.ToDayTime(),
                            std::move(rbuf));
}
f9twf_API void I072ClosingParserToRts(ExgMcMessage& e) {
   I071ClosingToRts(e, *static_cast<const ExgMcI072*>(&e.Pk_));
}
f9twf_API void I071ClosingParserToRts(ExgMcMessage& e) {
   I071ClosingToRts(e, *static_cast<const ExgMcI071*>(&e.Pk_));
}

} // namespaces
