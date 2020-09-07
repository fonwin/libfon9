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
                              MaTreeBase::Locker&& ulk) {
   (void)cmdln; (void)ulk;
   res.OpResult_ = OpResult::not_supported_cmd;
   resHandler(res, nullptr);
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
MaTree::PodOp::PodOp(ContainerImpl::value_type& v, Tree& sender, OpResult res, const StrView& key, Locker& locker)
   : base{*this, sender, res, key, locker}
   , Seed_{v} {
}
MaTree::PodOp::~PodOp() {
   if (this->IsWrote_) {
      this->Lock();
      static_cast<MaTree*>(this->Sender_)->OnMaTree_AfterUpdated(this->Locker_, *this->Seed_);
   }
}
void MaTree::PodOp::BeginWrite(Tab& tab, FnWriteOp fnCallback) {
   base::BeginWrite(tab, std::move(fnCallback));
   this->IsWrote_ = true;
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
   TreeOp_Get_MustLock<PodOp>(*this, static_cast<MaTree*>(&this->Tree_)->Container_,
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
