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
   fon9::TimeStamp   UpdatedTime_;
};
/// 使用 [pseq] 對照 P08 的內容.
struct P08Recs : public std::vector<P08Rec> {
   /// PA8(長Id) 的檔案時間.
   fon9::TimeStamp   UpdatedTimeL_;
   /// P08(短Id) 的檔案時間.
   fon9::TimeStamp   UpdatedTimeS_;

   const P08Rec* GetP08(TmpSymbolSeq_t pseq) const {
      if (fon9_LIKELY(pseq < this->size()))
         return &(*this)[pseq];
      return nullptr;
   }
   const P08Rec* GetP08(TmpSymbolSeq pseq) const {
      return this->GetP08(TmpGetValueU(pseq));
   }
};

/// 期交所提供給期貨商的一些基本資料對照表.
/// - P06: BrkId <-> FcmId 對照.
/// - P07: SocketId -> dn:Port 對照.
class f9twf_API ExgMapMgr : public fon9::seed::FileImpMgr {
   fon9_NON_COPY_NON_MOVE(ExgMapMgr);
   using base = fon9::seed::FileImpMgr;

   struct P06Loader;
   struct P07Loader;
   struct ImpSeedP08;

   struct MapsImpl {
      ExgMapBrkFcmId    MapBrkFcmId_;
      ExgMapSessionDn   MapSessionDn_[ExgSystemTypeCount()];
      P08Recs           MapP08Recs_[ExgSystemTypeCount()];
   };
   using Maps = fon9::MustLock<MapsImpl>;
   Maps              Maps_;
   fon9::TimeStamp   TDay_;

   static fon9::seed::FileImpTreeSP MakeSapling(ExgMapMgr& rthis);

protected:
   using MapsLocker = Maps::Locker;
   /// 當 P08 載入後的通知.
   /// - 載入後立即通知, 可能僅有 LongId_ 或 ShortId_ 或兩者都有.
   /// - 此時 lk 在鎖定狀態.
   /// - 預設 do nothing.
   virtual void OnP08Updated(const P08Recs& p08recs, ExgSystemType sysType, Maps::ConstLocker&& lk);
   virtual void OnP06Updated(const ExgMapBrkFcmId& mapBrkFcmId, Maps::Locker&& lk);

public:
   template <class... ArgsT>
   ExgMapMgr(ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...) {
      this->SetConfigSapling(MakeSapling(*this));
   }

   BrkId GetBrkId(FcmId id) const {
      Maps::ConstLocker maps{this->Maps_};
      return maps->MapBrkFcmId_.GetBrkId(id);
   }
   FcmId GetFcmId(BrkId id) const {
      Maps::ConstLocker maps{this->Maps_};
      return maps->MapBrkFcmId_.GetFcmId(id);
   }
   bool AppendDn(ExgSystemType sys, const ExgLineTmpArgs& lineArgs, std::string& devcfg) const;

   void SetTDay(fon9::TimeStamp tday);

   using MapsConstLocker = Maps::ConstLocker;
   MapsConstLocker Lock() const {
      return MapsConstLocker{this->Maps_};
   }
};
using ExgMapMgrSP = fon9::intrusive_ptr<ExgMapMgr>;

} // namespaces
#endif//__f9twf_ExgMapMgr_hpp__
