/// \file fon9/fmkt/TradingRequest.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/TradingRequest.hpp"

namespace fon9 { namespace fmkt {

TradingRxItem::~TradingRxItem() {
}
void TradingRxItem::FreeThis() {
   delete this;
}
void TradingRxItem::RevPrint(RevBuffer&) const {
}
const TradingRxItem* TradingRxItem::CastToRequest() const {
   return nullptr;
}
const TradingRxItem* TradingRxItem::CastToOrder() const {
   return nullptr;
}
const TradingRxItem* TradingRxItem::CastToEvent() const {
   return nullptr;
}
//--------------------------------------------------------------------------//
TradingRequest::~TradingRequest() {
}

} } // namespaces
