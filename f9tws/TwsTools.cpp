// \file f9tws/TwsTools.cpp
// \author fonwinz@gmail.com
#include "f9tws/TwsTools.hpp"

namespace f9tws {

/// 升降單位類別:
/// ---
/// 股票、債券換股權利證書、證券投資信託封閉式基金受益憑證、存託憑證、外國股票、
/// 新股權利證書、股款繳納憑證、附認股權特別股.
///   <10:  0.01
///   <50:  0.05
///   <100: 0.10
///   <500: 0.50
///   <1000:1.00
///   else: 5.00
const fon9::fmkt::LvPriStep   LvPriSteps_Stk[] = {
   {fon9::fmkt::Pri{  10,0}, fon9::fmkt::Pri{1,2}},
   {fon9::fmkt::Pri{  50,0}, fon9::fmkt::Pri{5,2}},
   {fon9::fmkt::Pri{ 100,0}, fon9::fmkt::Pri{1,1}},
   {fon9::fmkt::Pri{ 500,0}, fon9::fmkt::Pri{5,1}},
   {fon9::fmkt::Pri{1000,0}, fon9::fmkt::Pri{1,0}},
   {fon9::fmkt::Pri::max(),  fon9::fmkt::Pri{5,0}},
};
/// ---
/// 認購(售)權證、認股權憑證.
///   <5:   0.01
///   <10:  0.05
///   <50:  0.10
///   <100: 0.50
///   <500: 1.00
///   else: 5.00
const fon9::fmkt::LvPriStep   LvPriSteps_W[] = {
   {fon9::fmkt::Pri{   5,0}, fon9::fmkt::Pri{1,2}},
   {fon9::fmkt::Pri{  10,0}, fon9::fmkt::Pri{5,2}},
   {fon9::fmkt::Pri{  50,0}, fon9::fmkt::Pri{1,1}},
   {fon9::fmkt::Pri{ 100,0}, fon9::fmkt::Pri{5,1}},
   {fon9::fmkt::Pri{ 500,0}, fon9::fmkt::Pri{1,0}},
   {fon9::fmkt::Pri::max(),  fon9::fmkt::Pri{5,0}},
};
/// ---
/// 轉換公司債、附認股權公司債.
///   <150: 0.05
///   <1000:1.00
///   else: 5.00
const fon9::fmkt::LvPriStep   LvPriSteps_CB[] = {
   {fon9::fmkt::Pri{ 150,0}, fon9::fmkt::Pri{5,2}},
   {fon9::fmkt::Pri{1000,0}, fon9::fmkt::Pri{1,0}},
   {fon9::fmkt::Pri::max(),  fon9::fmkt::Pri{5,0}},
};
/// ---
/// 國內成分股指數股票型基金受益憑證(ETF)、國外成分股指數股票型基金受益憑證(ETF)、
/// 指數股票型期貨信託基金受益憑證(ETF)、槓桿反向指數股票型基金受益憑證(ETF)、
/// 境外指數股票型基金受益憑證(ETF)、受益證券(REITs)、國內成分指數投資證券(ETN)、
/// 國外成分指數投資證券(ETN)、槓桿反向型指數投資證券(ETN)
///   <50:  0.01
///   else: 0.05
const fon9::fmkt::LvPriStep   LvPriSteps_ETF[] = {
   {fon9::fmkt::Pri{50,0},   fon9::fmkt::Pri{1,2}},
   {fon9::fmkt::Pri::max(),  fon9::fmkt::Pri{5,2}},
};
/// ---
/// 公司債
///   0.05
const fon9::fmkt::LvPriStep   LvPriSteps_CoBonds[] = {
   {fon9::fmkt::Pri::max(),  fon9::fmkt::Pri{5,2}},
};
/// ---
/// 外國債券
/// 中央登錄公債 / Book-Entry Central Government Securities (CGS)
///   0.01 (0.01%) 升降單位為一個基本點（即1b.p.＝0.01%）;
const fon9::fmkt::LvPriStep   LvPriSteps_GBonds[] = {
   {fon9::fmkt::Pri::max(),  fon9::fmkt::Pri{1,2}},
};
//--------------------------------------------------------------------------//
void TwsSymbKindLvPriStep::Setup(StkNo stkno) {
   // 參考: https://www.twse.com.tw/downloads/zh/products/stock_cod.pdf
   // 及交易所的 T30 說明。
   this->LvPriSteps_ = LvPriSteps_Stk;
   this->Kind_ = f9tws_SymbKind_Unknown;

   const char     ch1 = stkno.Chars_[0];
   const char     ch2 = stkno.Chars_[1];
   const uint8_t  hdr2 = (fon9::isdigit(static_cast<unsigned char>(ch1)) && fon9::isdigit(static_cast<unsigned char>(ch2))
                          ? static_cast<uint8_t>(((ch1 - '0') * 10 + (ch2 - '0')))
                          : static_cast<uint8_t>(199));
   const char     ch5 = stkno.Chars_[4];
   const char     ch6 = stkno.Chars_[5];
   // -------------------------------------
   if (fon9_UNLIKELY(hdr2 == 91)) {
      switch (ch5) {
      case 'G':
         // 存託憑證 認股權憑證　　　　　　　　　　　　9100-9199 + 第五碼G + 第六碼1~9
         if ('1' <= ch6 && ch6 <= '9') {
            this->LvPriSteps_ = LvPriSteps_W;
            this->Kind_ = f9tws_SymbKind_DR_SW;
            return;
         }
         // 存託憑證 附認股權公司債　　　　　　　　　　9100-9199 + 第五碼G + 第六碼D~L
         if ('D' <= ch6 && ch6 <= 'L') {
            this->LvPriSteps_ = LvPriSteps_CB;
            this->Kind_ = f9tws_SymbKind_DR_CBW;
            return;
         }
         break;
      case 'C':
         // 存託憑證 可轉換公司債　　　　　　　　　　  9100-9199 + 第五碼C + 第六碼1~9
         if ('1' <= ch6 && ch6 <= '9') {
            this->LvPriSteps_ = LvPriSteps_CB;
            this->Kind_ = f9tws_SymbKind_DR_CB;
            return;
         }
         break;
      case 'F':
         // 存託憑證 附認股權公司債履約後之公司債　　　9100-9199 + 第五碼F + 第六碼1~9
         if ('1' <= ch6 && ch6 <= '9') {
            this->LvPriSteps_ = LvPriSteps_CoBonds;
            this->Kind_ = f9tws_SymbKind_DR_CBWP;
            return;
         }
         break;
      }
      // (保留)　 　　　　　　　　　　　　　　　　9201-9499.
      // 存託憑證TDR　　　　　　　　　　　　　　　910001-919999　　　　　(註3)
      // if ('0' <= ch5 && ch5 <= '9' && '0' <= ch6 && ch6 <= '9') {
         this->Kind_ = f9tws_SymbKind_TDR;
         return;
      // }
   }
   // -------------------------------------
   if (fon9_UNLIKELY(fon9::isdigit(static_cast<unsigned char>(ch5))
                     && ((3 <= hdr2 && hdr2 <= 8) || (70 <= hdr2 && hdr2 <= 73)))) {
      // 集 國內標的認購權證　　　　 030000-089999
      // 中 國內標的認售權證　　　　 03000-08999+第六碼P or U or T
      // 市 外國標的認購權證　　　　 03000-08999+第六碼F
      // 場 外國標的認售權證　　　　 03000-08999+第六碼Q
      // 權 下限型認購權證　　　　　 03000-08999+第六碼C
      // 證 上限型認售權證　　　　　 03000-08999+第六碼B
      // 　 可展延型下限型認購權證　 03000-08999+第六碼X
      // 　 可展延型上限型認售權證　 03000-08999+第六碼Y
      // -----
      // 櫃 國內標的認購權證　　　　 700000-739999
      // 買 國內標的認售權證　　　　 70000-73999+第六碼P or U or T
      // 中 外國標的認購權證　　　　 70000-73999+第六碼F
      // 心 外國標的認售權證　　　　 70000-73999+第六碼Q
      // 權 下限型認購權證　　　　　 70000-73999+第六碼C
      // 證 上限型認售權證　　　　　 70000-73999+第六碼B
      // 　 可展延型下限型認購權證　 70000-73999+第六碼X
      // 　 可展延型上限型認售權證　 70000-73999+第六碼Y
      this->LvPriSteps_ = LvPriSteps_W;
      switch (ch6) {
      default:
         if (isdigit(ch6)) {
            this->Kind_ = f9tws_SymbKind_WC;
            return;
         }
         break;
      case 'P':
      case 'U':
      case 'T':
         this->Kind_ = f9tws_SymbKind_WP;
         return;
      case 'F':
         this->Kind_ = f9tws_SymbKind_WCF;
         return;
      case 'Q':
         this->Kind_ = f9tws_SymbKind_WPF;
         return;
      case 'C':
         this->Kind_ = f9tws_SymbKind_WCBull;
         return;
      case 'B':
         this->Kind_ = f9tws_SymbKind_WCBear;
         return;
      case 'X':
         this->Kind_ = f9tws_SymbKind_WCBullE;
         return;
      case 'Y':
         this->Kind_ = f9tws_SymbKind_WCBearE;
         return;
      }
      // LvPriSteps_W
      // f9tws_SymbKind_Unknown;
      return;
   }
   // -------------------------------------
   if (fon9_LIKELY(10 <= hdr2 && hdr2 <= 99)) {
      // 一般股票　　　　　　　　　　　　1000 - 9999
      if (ch5 == ' ') {
         this->Kind_ = f9tws_SymbKind_Stock;
         return;
      }
      // 一般特別股　　　　　　　　　　　股票 + 第五碼A~Y(2022/06/06改,原為A~W)
      if ('A' <= ch5 && ch5 <= 'Y') {
         this->Kind_ = f9tws_SymbKind_PS;
         return;
      }
      // 2022/06/06 刪除 [可轉換公司債.換股權利證書], 新增[可交換特別股];
      // 換股權利證書　　　　　　　　　　股票 + 第五碼X~Z
      // if ('X' <= ch5 && ch5 <= 'Z') {
      //    this->Kind_ = f9tws_SymbKind_EC;
      //    return;
      // }
      // 可交換特別股                  股票 + 第五碼Z   + 第六碼1-9
      if (ch5 == 'Z' && ('1' <= ch6 && ch6 <= '9')) {
         this->Kind_ = f9tws_SymbKind_EPS;
         return;
      }

      // 交換公司債及交換金融債　　　　　股票 + 第五碼0   + 第六碼1~9
      if (ch5 == '0' && ('1' <= ch6 && ch6 <= '9')) {
         this->LvPriSteps_ = LvPriSteps_CoBonds; // ???
         this->Kind_ = f9tws_SymbKind_ECB;
         return;
      }
      // 轉換公司債　　　　　　　　　　　股票 + 第五碼1~9 + 第六碼空白,0~9
      // 二十三、於國內上市、上櫃、登錄興櫃[股票] 或 [公開發行之公司] 發行 [含股權性質] 之 [外幣計價債券]：
      //    (一) [轉換公司債]　　 或 [海外轉換公司債]：　　四位股票代號 + 一碼數字 1-9(可重複使用) + E。
      //    (二) [附認股權公司債] 或 [海外附認股權公司債]：四位股票代號 + 一碼數字 1-9(可重複使用) + W。
      if ('1' <= ch5 && ch5 <= '9') {
         switch (ch6) {
         default:
            if (!fon9::isdigit(static_cast<unsigned char>(ch6)))
               break;
            /* fall through */
         case ' ':
         case 'E':
         case 'W':
            this->LvPriSteps_ = LvPriSteps_CB;
            this->Kind_ = f9tws_SymbKind_CB;
            return;
         }
      }
      if ('G' == ch5) {
         // 認股權憑證　　　　　　　　　　　股票 + 第五碼G   + 第六碼1~9
         if ('1' <= ch6 && ch6 <= '9') {
            this->LvPriSteps_ = LvPriSteps_W;
            this->Kind_ = f9tws_SymbKind_SW;
            return;
         }
         // 附認股權特別股　　　　　　　　　股票 + 第五碼G   + 第六碼A~C
         if ('A' <= ch6 && ch6 <= 'C') {
            this->Kind_ = f9tws_SymbKind_PSW;
            return;
         }
         // 附認股權公司債　　　　　　　　　股票 + 第五碼G   + 第六碼D~L
         if ('D' <= ch6 && ch6 <= 'L') {
            this->LvPriSteps_ = LvPriSteps_CB;
            this->Kind_ = f9tws_SymbKind_CBW;
            return;
         }
      }
      // 附認股權公司債_履約後之公司債　　股票 + 第五碼F   + 第六碼1~9
      if ('F' == ch5) {
         if ('1' <= ch6 && ch6 <= '9') {
            this->LvPriSteps_ = LvPriSteps_CoBonds;
            this->Kind_ = f9tws_SymbKind_CBWP;
            return;
         }
      }
   }
   // -------------------------------------
   if (hdr2 == 1) {
      this->LvPriSteps_ = LvPriSteps_ETF;
      switch (ch6) {
      case 'P':
         // 不動產資產信託受益證券　　　01000-01999+第六碼P
         this->Kind_ = f9tws_SymbKind_REAT;
         return;
      case 'S':
         // 金融資產證券化受益證券　　　01000-01999+第六碼S
         this->Kind_ = f9tws_SymbKind_FABS;
         return;
      case 'T':
         // 不動產投資信託受益證券　　　01000-01999+第六碼T
         this->Kind_ = f9tws_SymbKind_REIT;
         return;
      }
      // LvPriSteps_ETF
      // f9tws_SymbKind_Unknown;
      return;
   }
   // -------------------------------------
   if (hdr2 == 0) {
      // 受益憑證 (封閉式基金)　　　 000101-004999　　　　　(註3)
      if (stkno.Chars_[2] <= '4') {
         this->Kind_ = f9tws_SymbKind_BC;
         return;
      }
      // ETF　　　　　　　　　　　　 00501-00999+第六碼空白 (註4)
      // ETF(外幣計價）　　　　　　　00501-00999+第六碼K
      // 債券ETF　　　　　　　　　　 00501-00999+第六碼B
      // 債券ETF(外幣計價）　　　　　00501-00999+第六碼C
      // 槓桿型ETF　　　　　　　　　 00501-00999+第六碼L
      // 槓桿型ETF(外幣計價）　　　　00501-00999+第六碼M
      // 反向型ETF　　　　　　　　　 00501-00999+第六碼R
      // 反向型ETF(外幣計價）　　　　00501-00999+第六碼S
      // 期信託ETF　　　　　　　　　 00501-00999+第六碼U
      // 期信託ETF(外幣計價）　　　　00501-00999+第六碼V
      this->LvPriSteps_ = LvPriSteps_ETF;
      switch (ch6) {
      default:
      case ' ':   this->Kind_ = f9tws_SymbKind_ETF;      return;
      case 'K':   this->Kind_ = f9tws_SymbKind_F_ETF;    return;
      case 'B':   this->Kind_ = f9tws_SymbKind_ETF_B;    return;
      case 'C':   this->Kind_ = f9tws_SymbKind_F_ETF_B;  return;
      case 'L':   this->Kind_ = f9tws_SymbKind_ETF_L;    return;
      case 'M':   this->Kind_ = f9tws_SymbKind_F_ETF_L;  return;
      case 'R':   this->Kind_ = f9tws_SymbKind_ETF_I;    return;
      case 'S':   this->Kind_ = f9tws_SymbKind_F_ETF_I;  return;
      case 'U':   this->Kind_ = f9tws_SymbKind_ETF_F;    return;
      case 'V':   this->Kind_ = f9tws_SymbKind_F_ETF_F;  return;
      }
   }
   // -------------------------------------
   if (hdr2 == 2) {
      // ETN　　　　　　　　　　　　 020000-029999
      // 債券型ETN　　　　　　　　　 02000-02999+第六碼B
      // 槓桿型ETN　　　　　　　　　 02000-02999+第六碼L
      // 反向型ETN　　　　　　　　　 02000-02999+第六碼R
      // 期權型ETN　　　　　　　　　 02000-02999+第六碼S
      this->LvPriSteps_ = LvPriSteps_ETF;
      switch (ch6) {
      default:    this->Kind_ = f9tws_SymbKind_ETN;      return;
      case 'B':   this->Kind_ = f9tws_SymbKind_ETN_B;    return;
      case 'L':   this->Kind_ = f9tws_SymbKind_ETN_L;    return;
      case 'R':   this->Kind_ = f9tws_SymbKind_ETN_I;    return;
      case 'S':   this->Kind_ = f9tws_SymbKind_ETN_S;    return;
      }
   }
   // 註3：所訂之受益憑證(封閉式基金)、存託憑證(TDR)六碼編碼原則，適用於 98年12月15日以後新上市之證券，以前編定之證券代號仍以維持4碼為原則。
   // 註4：所訂之ETF編碼原則，適用於103年6月24日以後新上市之證券，以前編定之證券代號仍以維持4碼或6碼為原則。
   // 註5：09開頭證券編碼保留。
   // -------------------------------------
   switch (ch1) {
   case 'A':
   case 'H':
   case 'P':
   case 'I':
      // 中央政府公債為 A 後加五位數字（二碼年度別＋一碼債券別＋二碼期別）。
      // 地方政府公債為 H 後加五碼英數字:
      //    一碼 A：臺北市政府公債、B：高雄市政府公債、C：新北市政府公債、D：臺中市政府公債、E：臺南市政府公債、F : 桃園市政府公債
      //    二碼年度別＋二碼期別
      // 分割公債：一碼證券識別碼（P 代表分割本金公債、I 代表分割利息公債）＋二碼到期（民國）年＋一碼到期月＋二碼到期日。
      this->Kind_ = f9tws_SymbKind_GS;
      this->LvPriSteps_ = LvPriSteps_GBonds;
      return;
   case 'G':
      // 金融債券及分割金融債券： G 加五位文數字。
      //    金融債券五位文數字為一碼行業別＋二碼公司別＋二碼券別，
      //    分割金融債券五位文數字為二碼公司別＋二碼券別＋一碼本金或利息別。
      this->Kind_ = f9tws_SymbKind_BB;
      this->LvPriSteps_ = LvPriSteps_GBonds;
      return;
   case 'F':
      // 新台幣計價之[外國債券]及第二十二項以外之外幣計價債券： F 後加五位文數字。
      // 五位文數字為: 三碼公司別＋二碼券別；
      // 分割債券五位文數字為: 二碼公司別＋二碼券別＋一碼本金或利息別。
      this->Kind_ = f9tws_SymbKind_BF;
      this->LvPriSteps_ = LvPriSteps_GBonds;
      return;
   }
   // -------------------------------------
   // 二十六、開放式基金受益憑證： T 加五位文數字(二碼公司別 + 二碼基金別 + 一碼類別)。
   // -------------------------------------
   // 二十七、具證券性質之虛擬通貨(簡稱 STO)：
   //    (一) 分潤型虛擬通貨：前兩碼為 ST，後加四位流水編碼。
   //    (二) 債務型虛擬通貨：前兩碼為 ST，後加三碼流水編碼，第六碼為 D。
   // -------------------------------------
   // 不認識的種類:
   // LvPriSteps_Stk
   // f9tws_SymbKind_Unknown;
}

} // namespaces
