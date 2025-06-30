﻿/// \file fon9/seed/TreeOp.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/TreeOp.hpp"

namespace fon9 { namespace seed {

SubscribableOp::~SubscribableOp() {
}
OpResult SubscribableOp::CheckSubscribeRights(Tab& tab, const SeedVisitor& from) {
   (void)tab; (void)from;
   return OpResult::no_error;
}
OpResult SubscribableOp::Subscribe(SubConn* pSubConn, Tab&, FnSeedSubr) {
   return SubscribeUnsupported(pSubConn);
}
OpResult SubscribableOp::Unsubscribe(SubConn*, Tab&) {
   return OpResult::not_supported_subscribe;
}
OpResult SubscribableOp::SubscribeStream(SubConn* pSubConn, Tab&, StrView, FnSeedSubr) {
   return SubscribeStreamUnsupported(pSubConn);
}
OpResult SubscribableOp::UnsubscribeStream(SubConn*, Tab&) {
   return OpResult::not_supported_subscribe_stream;
}

TreeOp::~TreeOp() {
}
void TreeOp::Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback) {
   fnCallback(PodRemoveResult{this->Tree_,
              tab ? OpResult::not_supported_remove_seed : OpResult::not_supported_remove_pod,
              strKeyText,
              tab});
}
void TreeOp::Add(StrView strKeyText, FnPodOp fnCallback) {
   this->Get(strKeyText, std::move(fnCallback));
}
void TreeOp::Get(StrView strKeyText, FnPodOp fnCallback) {
   fnCallback(PodOpResult{this->Tree_, OpResult::not_supported_get_pod, strKeyText}, nullptr);
}
void TreeOp::GridView(const GridViewRequest& req, FnGridViewOp fnCallback) {
   GridViewResult res{this->Tree_, req.Tab_, OpResult::not_supported_grid_view};
   fnCallback(res);
}
void TreeOp::GridApplySubmit(const GridApplySubmitRequest& req, FnCommandResultHandler fnCallback) {
   SeedOpResult res{this->Tree_, OpResult::not_supported_grid_apply, StrView{}, req.Tab_};
   fnCallback(res, StrView{});
}

} } // namespaces
