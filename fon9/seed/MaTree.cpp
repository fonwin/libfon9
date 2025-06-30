/// \file fon9/seed/MaTree.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/MaTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace seed {

NamedSeed::~NamedSeed() {
}
void NamedSeed::OnParentTreeClear(Tree&) {
   if (TreeSP sapling = this->GetSapling())
      sapling->OnParentSeedClear();
}
void NamedSeed::OnNamedSeedReleased() {
   if (this->use_count() == 0)
      delete this;
}
Fields NamedSeed::MakeDefaultFields() {
   Fields fields;
   fields.Add(fon9_MakeField2(NamedSeed, Title));
   fields.Add(fon9_MakeField2(NamedSeed, Description));
   return fields;
}

//--------------------------------------------------------------------------//

void NamedSeed::OnSeedCommand(SeedOpResult& res, StrView cmdln,
                              FnCommandResultHandler resHandler,
                              MaTreeBase::Locker&& ulk,
                              SeedVisitor* visitor) {
   (void)cmdln; (void)ulk; (void)visitor;
   res.OpResult_ = OpResult::not_supported_cmd;
   resHandler(res, nullptr);
}
OpResult NamedSeed::FromPodOp_CheckSubscribeRights(Tab& tab, const SeedVisitor& from) {
   return OpResult::no_error;
}
OpResult NamedSeed::FromPodOp_Subscribe(const MaTreePodOp& lk, SubConn* pSubConn, Tab& tab, FnSeedSubr subr) {
   (void)lk; (void)tab; (void)subr;
   return SubscribeUnsupported(pSubConn);
}
OpResult NamedSeed::FromPodOp_Unsubscribe(const MaTreePodOp& lk, SubConn* pSubConn, Tab& tab) {
   (void)lk; (void)pSubConn; (void)tab;
   return OpResult::not_supported_subscribe;
}
OpResult NamedSeed::FromPodOp_SubscribeStream(const MaTreePodOp& lk, SubConn* pSubConn, Tab& tab, StrView args, FnSeedSubr subr) {
   (void)lk; (void)tab; (void)args; (void)subr;
   return SubscribeStreamUnsupported(pSubConn);
}
OpResult NamedSeed::FromPodOp_UnsubscribeStream(const MaTreePodOp& lk, SubConn* pSubConn, Tab& tab) {
   (void)lk; (void)pSubConn; (void)tab;
   return OpResult::not_supported_subscribe_stream;
}
TreeSP NamedSeed::GetSapling() {
   return nullptr;
}

NamedSapling::~NamedSapling() {
}
TreeSP NamedSapling::GetSapling() {
   return this->Sapling_;
}

NamedMaTree::~NamedMaTree() {
}
TreeSP NamedMaTree::GetSapling() {
   return this->Sapling_;
}

//--------------------------------------------------------------------------//

MaTree::MaTree(std::string tabName)
   : base{new Layout1(fon9_MakeField2(NamedSeed, Name),
                      new Tab{Named{std::move(tabName)}, NamedSeed::MakeDefaultFields()})} {
}

void MaTree::OnMaTree_AfterAdd(Locker&, NamedSeed&) {
}
void MaTree::OnMaTree_AfterRemove(Locker&, NamedSeed&) {
}
void MaTree::OnMaTree_AfterClear() {
}
void MaTree::OnMaTree_AfterUpdated(Locker&, NamedSeed&) {
}

bool MaTree::Add(NamedSeedSP seed, StrView logErrHeader) {
   {
      Locker container{this->Container_};
      auto   ires = container->insert(seed);
      if (ires.second) {
         this->OnMaTree_AfterAdd(container, **ires.first);
         return true;
      }
   } // unlock.
   if (!logErrHeader.empty())
      fon9_LOG_ERROR(logErrHeader, "|name=", seed->Name_, "|err=seed exists");
   return false;
}

std::vector<NamedSeedSP> MaTree::GetList(StrView nameHead) const {
   std::vector<NamedSeedSP>   res;
   ConstLocker                container{this->Container_};
   auto                       ifind{container->lower_bound(nameHead)};
   while (ifind != container->end()) {
      NamedSeed& seed = **ifind;
      if (seed.Name_.size() < nameHead.size()
      || memcmp(seed.Name_.c_str(), nameHead.begin(), nameHead.size()) != 0)
         break;
      res.emplace_back(&seed);
      ++ifind;
   }
   return res;
}

NamedSeedSP MaTree::Remove(StrView name) {
   Locker   container{this->Container_};
   auto     ifind{container->find(name)};
   if (ifind == container->end())
      return nullptr;
   NamedSeedSP seed = *ifind;
   container->erase(ifind);
   this->OnMaTree_AfterRemove(container, *seed);
   return seed;
}

void MaTree::OnParentSeedClear() {
   base::OnParentSeedClear();
   this->OnMaTree_AfterClear();
}

//--------------------------------------------------------------------------//

fon9_MSC_WARN_DISABLE(4355);/* 'this' : used in base member initializer list*/
MaTreePodOp::MaTreePodOp(const NamedSeedSP& v, Tree& sender, OpResult res, const StrView& key, MaTreeBase::Locker& locker)
   : base{*this, sender, res, key, locker}
   , Seed_{v} {
   assert(v.get() != nullptr);
}
MaTreePodOp::~MaTreePodOp() {
   if (this->IsWrote_) {
      this->Lock();
      static_cast<MaTree*>(this->Sender_)->OnMaTree_AfterUpdated(this->Locker_, *this->Seed_);
   }
}
void MaTreePodOp::BeginWrite(Tab& tab, FnWriteOp fnCallback) {
   base::BeginWrite(tab, std::move(fnCallback));
   this->IsWrote_ = true;
}
void MaTreePodOp::OnVisitorCommand(Tab* tab, StrView cmdln, FnCommandResultHandler resHandler, SeedVisitor& visitor) {
   this->Tab_ = tab;
   this->Unlock();
   this->Seed_->OnSeedCommand(*this, cmdln, std::move(resHandler), std::move(this->Locker_), &visitor);
}
OpResult MaTreePodOp::CheckSubscribeRights(Tab& tab, const SeedVisitor& from) {
   return this->Seed_->FromPodOp_CheckSubscribeRights(tab, from);
}
OpResult MaTreePodOp::Subscribe(SubConn* pSubConn, Tab& tab, FnSeedSubr subr) {
   return this->Seed_->FromPodOp_Subscribe(*this, pSubConn, tab, std::move(subr));
}
OpResult MaTreePodOp::Unsubscribe(SubConn* pSubConn, Tab& tab) {
   return this->Seed_->FromPodOp_Unsubscribe(*this, pSubConn, tab);
}
OpResult MaTreePodOp::SubscribeStream(SubConn* pSubConn, Tab& tab, StrView args, FnSeedSubr subr) {
   return this->Seed_->FromPodOp_SubscribeStream(*this, pSubConn, tab, args, std::move(subr));
}
OpResult MaTreePodOp::UnsubscribeStream(SubConn* pSubConn, Tab& tab) {
   return this->Seed_->FromPodOp_UnsubscribeStream(*this, pSubConn, tab);
}
// -----------------------------------
static void MakeNamedSeedView(NamedSeedContainerImpl::iterator ivalue, Tab* tab, RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, SimpleRawRd{**ivalue}, rbuf, GridViewResult::kCellSplitter);
   RevPrint(rbuf, (**ivalue).Name_);
}
void MaTree::TreeOp::GridView(const GridViewRequest& req, FnGridViewOp fnCallback) {
   TreeOp_GridView_MustLock(*this, static_cast<MaTree*>(&this->Tree_)->Container_,
                              req, std::move(fnCallback), &MakeNamedSeedView);
}
void MaTree::TreeOp::Get(StrView strKeyText, FnPodOp fnCallback) {
   TreeOp_Get_MustLock<MaTreePodOp>(*this, static_cast<MaTree*>(&this->Tree_)->Container_,
                                    strKeyText, std::move(fnCallback));
}
fon9_WARN_POP;
// -----------------------------------
void MaTree::OnTreeOp(FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}

//--------------------------------------------------------------------------//

fon9_API NamedSeedSP GetNamedSeedNode(MaTreeSP node, StrView path) {
   while (!path.empty()) {
      StrView nodeName = StrFetchNoTrim(path, '/');
      if (nodeName.empty())
         continue;
      if (path.empty())
         return node->Get(nodeName);
      node = node->GetSapling<MaTree>(nodeName);
      if (!node)
         break;
   }
   return nullptr;
}

} } // namespaces
