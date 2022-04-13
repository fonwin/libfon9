// \file f9twf/ExgMapMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMapMgr_hpp__
#define __f9twf_ExgMapMgr_hpp__
#include "f9twf/ExgMapSessionDn.hpp"
#include "fon9/seed/FileImpTree.hpp"

namespace f9twf {

struct TmpP08Fields {
   /// 交割年月 yyyymm.
   fon9::CharAry<6>  settle_date_;
   /// 履約價 9(5)V9999; 期貨填 0;
   fon9::CharAry<9>  strike_price_v4_;
   /// 買賣權別 C:call, P:put; 期貨:空白.
   fon9::CharAry<1>  call_put_code_;
   /// 上市日期(yyyymmdd).
   fon9::CharAry<8>  begin_date_;
   /// 下市日期(yyyymmdd).
   fon9::CharAry<8>  end_date_;

   // 109.08.03 移除漲跌停價, 改放在 P11/PB1 設定.
   // /// 第一漲停價, 參考商品價格欄位小數位數.
   // fon9::CharAry<9>  raise_price1_;
   // /// 第一跌停價, 參考商品價格欄位小數位數.
   // fon9::CharAry<9>  fall_price1_;

   /// 權利金(開盤參考價), 參考商品價格欄位小數位數.
   fon9::CharAry<9>  premium_;

   // /// 第二漲停價, 參考商品價格欄位小數位數.
   // fon9::CharAry<9>  raise_price2_;
   // /// 第二跌停價, 參考商品價格欄位小數位數.
   // fon9::CharAry<9>  fall_price2_;
   // /// 第三漲停價, 參考商品價格欄位小數位數.
   // fon9::CharAry<9>  raise_price3_;
   // /// 第三跌停價, 參考商品價格欄位小數位數.
   // fon9::CharAry<9>  fall_price3_;

   /// 契約種類 I:指數類 R:利率類 B:債券類 C:商品類 S:股票類 E:匯率類.
   fon9::CharAry<1>  prod_kind_;
   /// 是否可報價 Y:可報價 N:不可報價.
   fon9::CharAry<1>  accept_quote_flag_;
   /// 價格欄位小數位數.
   fon9::CharAry<1>  decimal_locator_;
   /// 商品序號.
   fon9::CharAry<5>  pseq_;
   /// 流程群組, 對應於 R11、R12 電文中的 flow_group_no 欄位.
   fon9::CharAry<2>  flow_group_;
   /// 最後結算日 yyyymmdd.
   fon9::CharAry<8>  delivery_date_;
   /// 履約價格小數位數, 選擇權商品代碼, 履約價格部份之小數位數, 期貨商品則值為0.
   fon9::CharAry<1>  strike_price_decimal_locator_;
   /// 市價單每筆委託最大口數.
   fon9::CharAry<3>  market_order_ceiling_;
};
struct TmpP08Base : public TmpP08Fields {
   fon9::CharAry<87> filler_;
};
struct TmpP08 : public ExgProdIdS, public TmpP08Base {
};
struct TmpPA8 : public ExgProdIdL, public TmpP08Base {
};
static_assert(sizeof(TmpP08Base) == 150, "struct TmpP08Base; must pack?");
static_assert(sizeof(TmpP08) == 160, "struct TmpP08; must pack?");
static_assert(sizeof(TmpPA8) == 170, "struct TmpPA8; must pack?");
//--------------------------------------------------------------------------//
struct P08Rec {
   using ShortId = fon9::CharAryL<sizeof(ExgProdIdS)>;
   using LongId = fon9::CharAryL<sizeof(ExgProdIdL)>;
   ShortId           ShortId_;
   LongId            LongId_;
   TmpP08Fields      Fields_;
   char              padding___[1];
   /// 最後異動時的檔案時間.
   fon9::TimeStamp   UpdatedTime_{fon9::TimeStamp::Null()};
};
/// 使用 [pseq] 對照 P08 的內容.
struct P08Recs : public std::vector<P08Rec> {
   /// PA8(長Id) 的檔案時間.
   fon9::TimeStamp   UpdatedTimeL_{fon9::TimeStamp::Null()};
   /// P08(短Id) 的檔案時間.
   fon9::TimeStamp   UpdatedTimeS_{fon9::TimeStamp::Null()};

   const P08Rec* GetP08(TmpSymbolSeq_t pseq) const {
      if (fon9_LIKELY(pseq < this->size()))
         return &(*this)[pseq];
      return nullptr;
   }
   const P08Rec* GetP08(TmpSymbolSeq pseq) const {
      return this->GetP08(TmpGetValueU(pseq));
   }
};
//--------------------------------------------------------------------------//
struct TmpP09Fields {
   char              NameBig5_[30];
   StkNo             stock_id_;
   /// 契約類別: I:指數類 R:利率類 B:債券類 C:商品類 S:股票類 E:匯率類;
   char              subtype_;
   /// 契約乘數. 9(7)V9(4);
   fon9::CharAry<11> contract_size_v4_;
   /// 狀態碼 N：正常, P：暫停交易, U：即將上市;
   char              status_code_;
   /// 幣別: 台幣(1)、美元(2)、歐元(3)、日幣(4)、英鎊(5)、澳幣(6)、港幣(7)、人民幣(8)...;
   char              currency_type_;
   /// 選擇權商品代碼履約價格部份.
   /// 股票類選擇權商品不適用此欄位，股票類選擇權商品代碼之履約價格小數位數請參考 P08/PA8 檔案.
   fon9::CharAry<1>  strike_price_decimal_locator_;
   /// 委託價格小數位數. 同 P08/PA8 商品價格小數位數.
   fon9::CharAry<1>  decimal_locator_;
   /// 是否可報價 Y:可報價, N:不可報價.
   char              accept_quote_flag_;
   /// 上市日期 日期格式為 YYYYMMDD 僅適用股票類契約.
   fon9::CharAry<8>  begin_date_;
   /// 是否可鉅額交易 Y:可, N:不可.
   char              block_trade_flag_;
   /// 到期別 S:標準 W:週.
   char              expiry_type_;
   /// 現貨類別 E:ETF, S:個股; 僅適用股票類契約.
   char              underlying_type_;
   /// 商品收盤時間群組
   fon9::CharAry<2>  market_close_group_;
   /// 一般交易時段：0, 盤後交易時段：1;
   char              end_session_;
};
struct TmpP09Base : public TmpP09Fields {
   fon9::CharAry<29> filler_;
};

struct ExgContractIdS {
   using Id = ContractId;
   Id  ContractId_{nullptr};
};
struct TmpP09 : public ExgContractIdS, public TmpP09Base {
};

// 目前不需要支援 PA9(Long ContractId).
// 如果真要支援 Long ContractId 可以從 P08Rec 處理:
// => Long  ContractId = P08Rec.LongId_ [0..7]
//    Short ContractId = P08Rec.ShortId_[0..2]
//    同一筆 P08Rec 必定參考同一個 Contract.
// struct ExgContractIdL {
//    using Id = fon9::CharAryF<8, char, ' '>;
//    Id  ContractId_{nullptr};
// };
// struct TmpPA9 : public ExgContractIdL, public TmpP09Base {
// };

using P09Rec = TmpP09;
using P09Recs = std::vector<P09Rec>;

//--------------------------------------------------------------------------//
/// 根據 sysType 的日夜盤, 設定預設的 sch 參數.
/// 日盤: 07:00-12:30(OMS換日後..夜盤開始前).
/// 夜盤: 13:00-05:00(夜盤開始前..OMS換日前).
f9twf_API void TwfSysImp_UnsafeSetDefaultSchCfg(ExgSystemType sysType, fon9::seed::FileImpSeed& imp);

/// 期交所提供給期貨商的一些基本資料對照表.
/// - P06: BrkId <-> FcmId 對照.
/// - P07: SocketId -> dn:Port 對照.
/// - P08: 商品基本資料.
/// - P09: 契約基本資料.
class f9twf_API ExgMapMgr : public fon9::seed::FileImpMgr {
   fon9_NON_COPY_NON_MOVE(ExgMapMgr);
   using base = fon9::seed::FileImpMgr;

   struct P06Loader;
   struct P07Loader;
   struct ImpSeedP08;
   struct ImpSeedP09;

   struct MapsImpl {
      ExgMapBrkFcmId    MapBrkFcmId_;
      ExgMapSessionDn   MapSessionDn_[ExgSystemTypeCount()];
      P08Recs           MapP08Recs_[ExgSystemTypeCount()];
      P09Recs           MapP09Recs_[ExgSystemTypeCount()];
   };
   using Maps = fon9::MustLock<MapsImpl>;
   Maps              Maps_;
   fon9::TimeStamp   TDay_;
   uint8_t           IsMainContractRefReadyBits_;
   char              Padding_______[7];

   static fon9::seed::FileImpTreeSP MakeSapling(ExgMapMgr& rthis);

protected:
   using MapsLocker = Maps::Locker;
   /// 當 P08 載入後的通知.
   /// - 載入後立即通知, 可能僅有 LongId_ 或 ShortId_ 或兩者都有.
   /// - 此時 lk 在鎖定狀態.
   /// - 預設 do nothing.
   virtual void OnP08Updated(const P08Recs& p08recs, ExgSystemType sysType, Maps::ConstLocker&& lk);
   virtual void OnP09Updated(const P09Recs& p09recs, ExgSystemType sysType, Maps::ConstLocker&& lk);
   virtual void OnP06Updated(const ExgMapBrkFcmId& mapBrkFcmId, Maps::Locker&& lk);

public:
   template <class... ArgsT>
   ExgMapMgr(ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...) {
      this->SetConfigSapling(MakeSapling(*this));
   }
   fon9::seed::FileImpTree& GetFileImpSapling() const {
      auto& retval = this->GetConfigSapling();
      assert(dynamic_cast<fon9::seed::FileImpTree*>(&retval) != nullptr);
      return *static_cast<fon9::seed::FileImpTree*>(&retval);
   };

   BrkId GetBrkId(FcmId id) const {
      Maps::ConstLocker maps{this->Maps_};
      return maps->MapBrkFcmId_.GetBrkId(id);
   }
   FcmId GetFcmId(BrkId id) const {
      Maps::ConstLocker maps{this->Maps_};
      return maps->MapBrkFcmId_.GetFcmId(id);
   }
   bool AppendDn(ExgSystemType sys, const ExgLineTmpArgs& lineArgs, std::string& devcfg) const;

   /// 初次啟動時, 必須在設定 TDay 之後, 才會根據排程載入 P08;
   /// 每次設定 TDay: 會先清除全部的 P08Recs、P09Recs, 然後全部重新載入(包含 P06,P07,P08,P09...);
   virtual void SetTDay(fon9::TimeStamp tday);

   using MapsConstLocker = Maps::ConstLocker;
   MapsConstLocker Lock() const {
      return MapsConstLocker{this->Maps_};
   }

   static void SetWaitingDescription(fon9::Named& impSeed, fon9::StrView cstrInfo, ExgSystemType sysType);
   bool IsP08Ready(ExgSystemType sys) const;
   bool IsP09Ready(ExgSystemType sys) const;
   /// 等候 sys 的 P08 載入;
   /// 如果 !IsReady, 則會 SetWaitingDescription(impSeed, "Waiting P08_xx", sys);
   bool IsP08Ready(ExgSystemType sys, const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const;
   /// 等候 sys 的 P09 載入;
   /// 如果 !IsReady, 則會 SetWaitingDescription(impSeed, "Waiting P09_xx", sys);
   bool IsP09Ready(ExgSystemType sys, const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const;
   /// 等候 FutNormal 及 OptNormal 的 P09 載入;
   /// 如果 !IsReady, 則會 SetWaitingDescription(impSeed, "Waiting P09_xx", sys);
   bool IsP09Ready(const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const {
      return(this->IsP09Ready(f9twf::ExgSystemType::FutNormal, lk, impSeed)
          && this->IsP09Ready(f9twf::ExgSystemType::OptNormal, lk, impSeed));
   }

   /// 檢查 ContractRef 是否已經載入?
   /// - 經由設定檔, 例:MXF.MainRef = TXF;
   /// - P13 的 日盤: P13.10 & P13.20;
   /// 如果沒有完成, 則 impSeed.SetDescription(); 設定等候訊息.
   bool IsMainContractRefReady(const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const;
   void SetContractRefReady() {
      this->IsMainContractRefReadyBits_ = static_cast<uint8_t>(this->IsMainContractRefReadyBits_ | (1 << f9twf::ExgSystemTypeCount()));
   }
   bool IsP13Ready(ExgSystemType sys, const ConfigLocker& lk, fon9::seed::FileImpSeed& impSeed) const;
   void SetP13Ready(f9twf::ExgSystemType sys) {
      this->IsMainContractRefReadyBits_ = static_cast<uint8_t>(this->IsMainContractRefReadyBits_ | (1 << f9twf::ExgSystemTypeToIndex(sys)));
   }

   // 期交所相關檔案(P06,P07/PA7,P08,PA8...), 預設: 當檔案有異動時, 自動重新載入.
   class f9twf_API ImpSeedBase : public fon9::seed::FileImpSeed {
      fon9_NON_COPY_NON_MOVE(ImpSeedBase);
      using base = fon9::seed::FileImpSeed;

   public:
      const ExgSystemType  ExgSystemType_{};
      char                 Padding____[7];

      template <class... ArgsT>
      ImpSeedBase(ExgSystemType systemType, fon9::seed::FileImpMonitorFlag mon, ArgsT&&... args)
         : base(mon, std::forward<ArgsT>(args)...)
         , ExgSystemType_{systemType} {
      }
      template <class... ArgsT>
      ImpSeedBase(ExgSystemType systemType, ArgsT&&... args)
         : base(fon9::seed::FileImpMonitorFlag::Reload, std::forward<ArgsT>(args)...)
         , ExgSystemType_{systemType} {
      }

      /// \ref TwfSysImp_UnsafeSetDefaultSchCfg();
      void SetDefaultSchCfg() {
         TwfSysImp_UnsafeSetDefaultSchCfg(this->ExgSystemType_, *this);
      }
      ExgMapMgr& GetExgMapMgr() const {
         return *static_cast<ExgMapMgr*>(&this->OwnerTree_.ConfigMgr_);
      }
      /// 必須等 P08 載入(ShortId), 才有價格小數位, 才能處理價格.
      /// 若 P08 尚未 ready, 則會設定 Description, 所以需要 lk.
      bool IsP08Ready(const ConfigLocker& lk) {
         return this->GetExgMapMgr().IsP08Ready(this->ExgSystemType_, lk, *this);
      }
      bool IsP09Ready(const ConfigLocker& lk) {
         return this->GetExgMapMgr().IsP09Ready(this->ExgSystemType_, lk, *this);
      }
      /// 預設 do nothing.
      void OnAfterLoad(fon9::RevBuffer& rbufDesp, fon9::seed::FileImpLoaderSP loader, fon9::seed::FileImpMonitorFlag monFlag) override;
   };
   class f9twf_API ImpSeedForceLoadSesNormal : public ImpSeedBase {
      fon9_NON_COPY_NON_MOVE(ImpSeedForceLoadSesNormal);
      using base = ImpSeedBase;
   public:
      using base::base;
      /// 如果不是 AfterHour, 則啟動時會強制載入, 例如:
      /// - 日盤P08: 即使是 SchOut 也要載入一次, 避免系統中沒有商品基本資料(價格小數位數), 無法正確處理價格.
      /// - 夜盤P08: 避免有前日的檔案殘留, 所以必須在 SchIn 才需要載入, 此時不用強制載入.
      void ClearReload(ConfigLocker&& lk) override;
   };

   // -------------------------
   #define ExgMapMgr_AddImpSeed_SNormal(configTree, impClass, fname, sysStr, sysName) \
      (configTree).Add(new impClass(f9twf::ExgSystemType::sysName##Normal,    configTree, fname "_" sysStr "0", "./tmpsave/" fname "." sysStr "0"));
   // -----
   #define ExgMapMgr_AddImpSeed_S2(configTree, impClass, fname, sysStr, sysName) \
      ExgMapMgr_AddImpSeed_SNormal(configTree, impClass, fname, sysStr, sysName);   \
      (configTree).Add(new impClass(f9twf::ExgSystemType::sysName##AfterHour, configTree, fname "_" sysStr "1", "./tmpsave/" fname "." sysStr "1"));
   // -----
   #define ExgMapMgr_AddImpSeed_S2FO(configTree, impClass, fname)     \
      ExgMapMgr_AddImpSeed_S2(configTree, impClass, fname, "1", Opt); \
      ExgMapMgr_AddImpSeed_S2(configTree, impClass, fname, "2", Fut);
   // -----
   #define ExgMapMgr_AddImpSeed_FO(configTree, impClass, fname)            \
      ExgMapMgr_AddImpSeed_SNormal(configTree, impClass, fname, "1", Opt); \
      ExgMapMgr_AddImpSeed_SNormal(configTree, impClass, fname, "2", Fut);
   // -------------------------
};
using ExgMapMgrSP = fon9::intrusive_ptr<ExgMapMgr>;

} // namespaces
#endif//__f9twf_ExgMapMgr_hpp__
