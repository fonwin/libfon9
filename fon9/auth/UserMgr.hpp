﻿/// \file fon9/auth/UserMgr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_UserMgr_hpp__
#define __fon9_auth_UserMgr_hpp__
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/Random.hpp"

namespace fon9 { namespace auth {

class fon9_API UserMgr;
using Salt = ByteVector;
using SaltedPass = ByteVector;
/// - PBKDF2 = iterator.
/// - Argon2 = (opslimit * 10000) + memlimit(MB). 尚未支援.
using PassAlgParam = uint64_t;

enum class HashPassFlag {
   ResetNone = 0,
   ResetSalt = 0x01,
   ResetAlgParam = 0x02,
   ResetAll = 0xff,
};
fon9_ENABLE_ENUM_BITWISE_OP(HashPassFlag);

struct PassRec {
   PassAlgParam   AlgParam_ = 0;
   Salt           Salt_;
   SaltedPass     SaltedPass_;

   /// HashAlg 可參考 fon9::crypto::Sha256:
   /// \code
   /// HashAlg::kOutputSize;
   /// HashAlg::CalcSaltedPassword(const void* pass, size_t passLen,
   ///                             const void* salt, size_t saltLen,
   ///                             size_t iter,
   ///                             size_t outLen, void* out);
   /// \endcode
   ///
   /// HashParam:
   /// \code
   ///   enum HashParam {
   ///      kSaltBytes = 12,
   ///      kAlgParam = 4096, // e.g. SHA-SCRAM-256 = ITERATOR
   ///   };
   /// \endcode
   template <class HashAlg, class HashParam>
   static bool HashPass(const char* pass, size_t passLen, PassRec& passRec, HashPassFlag flags) {
      if (IsEnumContains(flags, HashPassFlag::ResetSalt))
         RandomBytes(passRec.Salt_.alloc(HashParam::kSaltBytes), HashParam::kSaltBytes);
      if (IsEnumContains(flags, HashPassFlag::ResetAlgParam))
         passRec.AlgParam_ = HashParam::kAlgParam;
      if (void* pSaltedPass = passRec.SaltedPass_.alloc(HashAlg::kOutputSize))
         return HashAlg::CalcSaltedPassword(pass, passLen,
                                            passRec.Salt_.begin(), passRec.Salt_.size(),
                                            passRec.AlgParam_,
                                            HashAlg::kOutputSize, pSaltedPass);
      return false;
   }
};
typedef bool (*FnHashPass)(const char* pass, size_t passLen, PassRec& passRec, HashPassFlag flags);

struct UserEv {
   TimeStamp   Time_ = TimeStamp::Null();
   std::string From_;
};

struct fon9_API UserRec : public PolicyItem {
   fon9_NON_COPY_NON_MOVE(UserRec);
   using base = PolicyItem;

protected:
   void LoadPolicy(DcQueue&) override;
   void SavePolicy(RevBuffer&) override;
   void OnSeedCommand(PolicyMaps::Locker& locker, seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler) override;
   StrView GetSeedCommandLogStr(StrView cmdln) override;

public:
   using base::base;

   PassRec     Pass_;
   RoleId      RoleId_;

   /// 帳號啟用時間.
   TimeStamp   NotBefore_ = TimeStamp::Null();
   /// 帳號截止時間.
   TimeStamp   NotAfter_ = TimeStamp::Null();

   /// 密碼最後異動時間/來自何方?
   UserEv      EvChgPass_;
   /// 最後認證成功的時間/來自何方?
   UserEv      EvLastAuth_;
   /// 最後密碼錯誤時間/來自何方? 即使後來登入成功,此值不會改變.
   UserEv      EvLastErr_;
   /// 密碼錯誤次數, 一旦認證成功(或改密碼成功)此值會歸零.
   uint8_t     ErrCount_ = 0;
   /// 密碼有效期限, 超過時間必須更改密碼才能登入.
   /// 0=不限;
   uint8_t     ExpDays_ = 0;
   uint8_t     Padding_FilledForAlign___[2];
   UserFlags   UserFlags_ = UserFlags::NeedChgPass;
   /// 配合 UserFlags::AllowBeAuthz; 檢查: 有哪些 authc 允許使用此 UserRec 當成 authz;
   /// 使用 ';' 分隔, 不理會空白;
   CharVector  AuthcList_;
};

//--------------------------------------------------------------------------//

class fon9_API UserTree : public PolicyTree {
   fon9_NON_COPY_NON_MOVE(UserTree);
   using base = PolicyTree;
protected:
   PolicyItemSP MakePolicy(const StrView& userId) override {
      return PolicyItemSP{new UserRec{userId}};
   }

public:
   using Locker = PolicyMaps::Locker;
   const FnHashPass  FnHashPass_;

   UserTree(seed::Fields&& fields, FnHashPass fnHashPass)
      : base("User", "UserId", std::move(fields),
             seed::TabFlag::Writable | seed::TabFlag::NoSapling | seed::TabFlag::HasSameCommandsSet)
      , FnHashPass_{std::move(fnHashPass)} {
   }

   using LockedUser = std::pair<Locker, UserRec*>;
   /// 僅取出授權用的使用者資料, 不檢查 Authc/Authz 的相關權限;
   LockedUser GetLockedUserForAuthz(const AuthResult& authr);

   /// - passRec:
   ///   - rcode = fon9_Auth_PassChanged  則必須提供 passRec, 若沒提供則會直接 crash!
   ///   - rcode != fon9_Auth_PassChanged 則不理會 passRec
   virtual AuthR AuthUpdate(fon9_Auth_R        rcode,
                            const AuthRequest& req,
                            AuthResult&        authr,
                            const PassRec*     passRec,
                            UserMgr&           owner);
};

/// \ingroup auth
/// 使用者資料&密碼管理.
/// - 使用 InnDbf 儲存設定.
/// - 需額外提供:
///   - Alg: PBKDF2?
///   - Salt size
///   - SaltedPass size
class fon9_API UserMgr : public PolicyAgent {
   fon9_NON_COPY_NON_MOVE(UserMgr);
   using base = PolicyAgent;
public:
   static seed::Fields MakeFields();

   UserMgr(const seed::MaTree* authMgrAgents, std::string name, FnHashPass fnHashPass);

   template <class... NamedSeedArgsT>
   UserMgr(seed::TreeSPT<UserTree> sapling, NamedSeedArgsT&&... namedSeedArgs)
      : base(std::move(sapling), std::forward<NamedSeedArgsT>(namedSeedArgs)...) {
   }

   static intrusive_ptr<UserMgr> Plant(AuthMgr& authMgr, std::string name, FnHashPass fnHashPass) {
      auto res = authMgr.Agents_->Plant<UserMgr>("UserMgr.Plant", std::move(name), std::move(fnHashPass));
      if (res)
         res->LinkStorage(authMgr.Storage_, sizeof(UserRec));
      return res;
   }

   using LockedUser = UserTree::LockedUser;
   LockedUser GetLockedUserForAuthz(const AuthResult& uid) {
      return static_cast<UserTree*>(this->Sapling_.get())->GetLockedUserForAuthz(uid);
   }

   /// 如果登入成功, 則會更新 authr.ExtInfo_
   AuthR AuthUpdate(fon9_Auth_R rcode, const AuthRequest& req, AuthResult& authr) {
      return static_cast<UserTree*>(this->Sapling_.get())->AuthUpdate(rcode, req, authr, nullptr, *this);
   }
   AuthR PassChanged(const PassRec& passRec, const AuthRequest& req, AuthResult& authr) {
      return static_cast<UserTree*>(this->Sapling_.get())->AuthUpdate(fon9_Auth_PassChanged, req, authr, &passRec, *this);
   }
   /// 檢查密碼(密碼存放在 AuthRequest::Response_);
   /// 用於某些不支援 SASL 的處理程序(例如:FIX).
   /// 如果密碼正確, 則會設定 authr.RoleId_ = (user->RoleId_.empty() ? authr.AuthcId_ : user->RoleId_);
   AuthR CheckLogon(const AuthRequest& req, AuthResult& authr) {
      return static_cast<UserTree*>(this->Sapling_.get())->AuthUpdate(fon9_Auth_CheckLogon, req, authr, nullptr, *this);
   }
};
using UserMgrSP = intrusive_ptr<UserMgr>;

} } // namespaces
#endif//__fon9_auth_UserMgr_hpp__
