/// \file fon9/auth/PolicySocketFrom.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/PolicySocketFrom.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace auth {

void SocketAddressRangeStr::OnStringAfterChanged() {
   StrTo(ToStrView(*this), this->Range_);
   char  buf[io::kMaxTcpConnectionUID];
   this->String_.assign(ToStrRev(buf + sizeof(buf), this->Range_), buf + sizeof(buf));
}

//--------------------------------------------------------------------------//

template <class Archive>
static void SerializeVer(Archive& ar, ArchiveWorker<Archive, PolicySocketAddressRangeRec>& rec, unsigned ver) {
   (void)ver; assert(ver == 0);
   ar(rec.AddressRange_,
      rec.IsAllow_);
}

template <class Archive>
static void RecSerialize(const Archive& ar, ArchiveWorker<Archive, PolicySocketAddressRangeRec>& rec) {
   CompositeVer<decltype(rec)> vrec{rec, 0};
   ar(vrec);
}

void PolicySocketAddressRangeRec::LoadPolicy(DcQueue& buf) {
   RecSerialize(BitvInArchive{buf}, *this);
}
void PolicySocketAddressRangeRec::SavePolicy(RevBuffer& rbuf) {
   RecSerialize(BitvOutArchive{rbuf}, *this);
}

seed::Fields PolicySocketAddressRangeRec::MakeFields() {
   seed::Fields fields;
   fields.Add(fon9_MakeField2(PolicySocketAddressRangeRec, AddressRange));
   fields.Add(fon9_MakeField2(PolicySocketAddressRangeRec, IsAllow));
   return fields;
}

//--------------------------------------------------------------------------//

class PolicySocketFromAgent::Tree : public PolicyTree {
   fon9_NON_COPY_NON_MOVE(Tree);
   using base = PolicyTree;

protected:
   PolicyItemSP MakePolicy(const StrView& key) override {
      return PolicyItemSP{new PolicySocketAddressRangeRec{key}};
   }

public:
   Tree() : base("SoAddr", "CheckSeq", PolicySocketAddressRangeRec::MakeFields(),
                 seed::TabFlag::NoSapling_NoSeedCommand_Writable) {
   }
   friend class PolicySocketFromAgent;
};

PolicySocketFromAgent::PolicySocketFromAgent(const seed::MaTree* authMgrAgents, std::string name)
   : base(new Tree, std::move(name)) {
   (void)authMgrAgents;
}

bool PolicySocketFromAgent::OnBeforeAccept(const io::SocketAddress& addrRemote, io::SocketResult& soRes, CharVector& exlog) {
   const size_t         szName = this->Name_.size();
   PolicyMaps::Locker   maps{static_cast<Tree*>(this->Sapling_.get())->PolicyMaps_};
   char* pex;
   for (auto& iar : maps->ItemMap_) {
      if (static_cast<const PolicySocketAddressRangeRec*>(iar.get())->AddressRange_.InRange(addrRemote)) {
         const size_t szPoId = iar->PolicyId_.size();
         pex = static_cast<char*>(exlog.alloc(3 + szName + 1 + szPoId));
         memcpy(pex + 3, this->Name_.c_str(), szName);
         *(pex + 3 + szName) = '/';
         memcpy(pex + 3 + szName + 1, iar->PolicyId_.begin(), szPoId);
         if (static_cast<const PolicySocketAddressRangeRec*>(iar.get())->IsAllow_ == EnabledYN::Yes) {
            memcpy(pex, "|A=", 3);
            return true;
         }
         soRes = io::SocketResult{"SocketFrom.Deny", std::errc::permission_denied};
         memcpy(pex, "|B=", 3);
         return false;
      }
   }
   soRes = io::SocketResult{"SocketFrom.NotFound", std::errc::permission_denied};
   pex = static_cast<char*>(exlog.alloc(3 + szName));
   memcpy(pex + 3, this->Name_.c_str(), this->Name_.size());
   memcpy(pex, "|B=", 3);
   return false;
}

} } // namespaces
