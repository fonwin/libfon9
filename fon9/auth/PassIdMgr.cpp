/// \file fon9/auth/PassIdMgr.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/PassIdMgr.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/Random.hpp"
#include "fon9/Log.hpp"
#include "fon9/PassKey.hpp"

namespace fon9 { namespace auth {

template <class Archive>
static void SerializeVer(Archive& ar, ArchiveWorker<Archive, PassIdRec>& rec, unsigned ver) {
   (void)ver; assert(ver == 0);
   ar(rec.SaltedPass_,
      rec.ChangedTime_
   );
}

template <class Archive>
static void PassIdRecSerialize(const Archive& ar, ArchiveWorker<Archive, PassIdRec>& rec) {
   CompositeVer<decltype(rec)> vrec{rec, 0};
   ar(vrec);
}

void PassIdRec::LoadPolicy(DcQueue& buf) {
   PassIdRecSerialize(BitvInArchive{buf}, *this);
}
void PassIdRec::SavePolicy(RevBuffer& rbuf) {
   PassIdRecSerialize(BitvOutArchive{rbuf}, *this);
}
//--------------------------------------------------------------------------//
// salt = 對稱加密的Key.
// 僅用簡單的 XOR 方式加解密, 因為:
// 1.若全部[設定檔]被偷走, 其實很容易被破解.
// 2.在這裡加密的意義是: 讓 [非有心人士] 無法直接看到明碼.
// 3.所以即使用 AES 加密, 甚至使用非對稱加密, 都沒有太大的意義.
static void EncodePass(ByteVector& saltedPass, StrView salt) {
   const auto  saltSz = salt.size();
   unsigned    saltIdx = 0;
   for (auto& ch : saltedPass) {
      ch = static_cast<fon9::byte>(ch ^ (salt.begin()[saltIdx % saltSz]));
      ++saltIdx;
   }
}
static bool DecodePass(const ByteVector& saltedPass, StrView salt, CharVector& password) {
   password.assign(saltedPass.begin(), saltedPass.end());
   const auto  saltSz  = salt.size();
   unsigned    saltIdx = 0;
   for (char& ch : password) {
      ch = ch ^ (salt.begin()[saltIdx % saltSz]);
      if (ch == 0) {
         password.resize(saltIdx);
         break;
      }
      if (fon9::isnotprint(ch))
         return false;
      ++saltIdx;
   }
   return true;
}

#define kCHAR_PassKeySpliter     '/'
void PassIdRec::OnSeedCommand(PolicyMaps::Locker& locker, seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler) {
   StrView cmd = StrFetchTrim(cmdln, &isspace);
   StrTrim(&cmdln);

   #define kCSTR_repw   "repw"
   if (cmd == kCSTR_repw) {
      // repw        建立:亂數密碼 & Salt
      // repw pass   使用 pass, 建立 Salt
      static const char kCSTR_LOG_repw[] = "PassIdRec.OnSeedCommand." kCSTR_repw "|passId=";
      const int saltSize = PassIdMgr::kPassKeyMaxLength - static_cast<int>(this->PolicyId_.size() + 1); // +1 = kCHAR_PassKeySpliter
      if (saltSize < 3) { // salt 最少 3 碼.
         locker.unlock();
         res.OpResult_ = seed::OpResult::value_overflow;
         resHandler(res, "passId too long");
         fon9_LOG_ERROR(kCSTR_LOG_repw, this->PolicyId_, "|err=passId too long");
         return;
      }
      this->ChangedTime_ = UtcNow();
      auto saltChk = this->ChangedTime_.GetOrigValue();

      CharVector saltStr;
      saltStr.resize(unsigned_cast(saltSize));
      char* pSaltPut = saltStr.end();
      pSaltPut[-1] = Seq2Alpha(static_cast<uint8_t>(saltChk % kSeq2AlphaSize));
      saltChk /= kSeq2AlphaSize;
      pSaltPut[-2] = Seq2Alpha(static_cast<uint8_t>(saltChk % kSeq2AlphaSize));
      saltChk /= kSeq2AlphaSize;

      pSaltPut = saltStr.begin();
      pSaltPut[0] = Seq2Alpha(static_cast<uint8_t>(saltChk % kSeq2AlphaSize));
      if (const auto rndSize = saltStr.size() - 3) {
         RandomChars(pSaltPut + 1, rndSize);
      }
      StrView  newPass;
      char     passbuf[12];
      if (cmdln.empty()) {
         RandomChars(passbuf, sizeof(passbuf));
         newPass.Reset(passbuf, passbuf + sizeof(passbuf));
      }
      else {
         newPass = fon9::SbrFetchInsideNoTrim(cmdln, StrBrArg::Quotation_);
         if (newPass.IsNull())
            newPass = cmdln;
      }
      auto iPassSize = newPass.size();
      this->SaltedPass_.assign(newPass.begin(), iPassSize);
      constexpr unsigned kMinSaltedPass = 64;
      if (iPassSize < kMinSaltedPass) {
         this->SaltedPass_.resize(kMinSaltedPass);
         this->SaltedPass_.begin()[iPassSize] = '\0';
         if (++iPassSize < kMinSaltedPass)
            RandomBytes(this->SaltedPass_.begin() + iPassSize, kMinSaltedPass - iPassSize);
      }
      EncodePass(this->SaltedPass_, ToStrView(saltStr));

      this->PassKey_.reserve(PassIdMgr::kPassKeyMaxLength);
      this->PassKey_ = this->PolicyId_;
      this->PassKey_.push_back(kCHAR_PassKeySpliter);
      this->PassKey_.append(saltStr.begin(), saltStr.end());
      std::string msg = "The new password=";
      newPass.AppendTo(msg);
      msg.append("\n" "PassKey=");
      this->PassKey_.AppendTo(msg);

      locker->WriteUpdated(*this);
      locker.unlock();

      res.OpResult_ = seed::OpResult::no_error;
      res.LogStr_ = "PassKey changed.";
      resHandler(res, &msg);
      fon9_LOG_INFO(kCSTR_LOG_repw, this->PolicyId_);
      return;
   }
   locker.unlock();
   if (cmd == "?") {
      res.OpResult_ = seed::OpResult::no_error;
      resHandler(res,
                 kCSTR_repw fon9_kCSTR_CELLSPL "Reset password" fon9_kCSTR_CELLSPL "[NewPass] or Random new password.");
      return;
   }
   res.OpResult_ = seed::OpResult::not_supported_cmd;
   resHandler(res, cmd);
}
StrView PassIdRec::GetSeedCommandLogStr(StrView cmdln) {
   constexpr size_t kCSTR_repw_size = sizeof(kCSTR_repw) - 1;
   if (cmdln.size() >= kCSTR_repw_size && memcmp(cmdln.begin(), kCSTR_repw, kCSTR_repw_size) == 0)
      return StrView{kCSTR_repw};
   return base::GetSeedCommandLogStr(cmdln);
}
//--------------------------------------------------------------------------//
seed::Fields PassIdRec::MakeFields() {
   seed::Fields fields;
   fields.Add(fon9_MakeField2_const(PassIdRec, ChangedTime));
   fields.Add(fon9_MakeField2_const(PassIdRec, PassKey));
   return fields;
}
//--------------------------------------------------------------------------//
PassIdTree::PassIdTree()
   : base("PassMap", "PassId", PassIdRec::MakeFields(),
          seed::TabFlag::Writable | seed::TabFlag::NoSapling | seed::TabFlag::HasSameCommandsSet) {
}
bool PassIdTree::GetPass(StrView passKey, CharVector& password) const {
   // passKey = "PassId/Salt"
   auto  passId = StrFetchTrim(passKey, kCHAR_PassKeySpliter); // 剩餘的 passKey = Salt
   auto  container = this->PolicyMaps_.Lock();
   auto  ifind = container->ItemMap_.find(passId);
   if (ifind == container->ItemMap_.end() || passKey.size() < 3)
      return false;
   const auto& passIdRec = *static_cast<const PassIdRec*>(ifind->get());
   auto        saltChk = passIdRec.ChangedTime_.GetOrigValue();
   const char* pSaltChk = passKey.end();
   if (pSaltChk[-1] != Seq2Alpha(static_cast<uint8_t>(saltChk % kSeq2AlphaSize)))
      return false;
   saltChk /= kSeq2AlphaSize;
   if (pSaltChk[-2] != Seq2Alpha(static_cast<uint8_t>(saltChk % kSeq2AlphaSize)))
      return false;
   saltChk /= kSeq2AlphaSize;
   pSaltChk = passKey.begin();
   if (pSaltChk[0] != Seq2Alpha(static_cast<uint8_t>(saltChk % kSeq2AlphaSize)))
      return false;
   return DecodePass(passIdRec.SaltedPass_, passKey, password);
}
//--------------------------------------------------------------------------//
PassIdMgr::PassIdMgr(const seed::MaTree* authMgrAgents, std::string name)
   : base(new PassIdTree(), std::move(name)) {
   (void)authMgrAgents;
}
//--------------------------------------------------------------------------//
static PassIdMgrSP gPassIdMgr;
static bool PassIdMgr_PasssKeyToPassword(const StrView& passKey, CharVector& password) {
   if (gPassIdMgr && gPassIdMgr->GetPass(passKey, password))
      return true;
   password.assign(passKey);
   return false;
}

PassIdMgrSP PassIdMgr::PlantPassKeyMgr(AuthMgr& authMgr, std::string name) {
   PassIdMgrSP retval = Plant(authMgr, std::move(name));
   if (retval) {
      gPassIdMgr = retval;
      SetFnPassKeyToPassword(&PassIdMgr_PasssKeyToPassword);
   }
   return retval;
}

} } // namespaces
