/// \file fon9/fmkt/TradingRequest.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/TradingRequest.hpp"
#include "fon9/fmkt/TradingLine.hpp"

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
bool TradingRequest::PreOpQueuingRequest(TradingLineManager& from) const {
   (void)from;
   return false;
}
TradingRequest::OpQueuingRequestResult TradingRequest::OpQueuingRequest(TradingLineManager& from,
                                                                        TradingRequest& queuingRequest) {
   (void)from; (void)queuingRequest;
   return Op_NotSupported;
}
bool TradingRequest::IsPreferTradingLine(TradingLine& tline) const {
   return tline.IsOrigSender(*this);
}

} } // namespaces
