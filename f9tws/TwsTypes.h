// \file f9tws/TwsTypes.h
// \author fonwinz@gmail.com
#ifndef __f9tws_TwsTypes_h__
#define __f9tws_TwsTypes_h__
#include "f9tws/Config.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

fon9_ENUM(f9tws_SymbKind, uint8_t) {
   f9tws_SymbKind_Unknown = 0,

   /// 一般股票 / Common stock.
   f9tws_SymbKind_Stock = 0x01,
   /// 一般特別股 / Preferred stock.
   f9tws_SymbKind_PS = 0x02,
   /// 換股權利證書 / Entitled certificate.
   f9tws_SymbKind_EC = 0x03,
   /// 附認股權特別股 / Preferred shares with warrants.
   f9tws_SymbKind_PSW = 0x04,
   /// 受益憑證(封閉式基金) / Beneficiary certificate (Close-end funds).
   f9tws_SymbKind_BC = 0x05,
   /// 可交換特別股 / Exchangeable preferred stock.
   f9tws_SymbKind_EPS = 0x06,

   /// ETF.
   f9tws_SymbKind_ETF = 0x10,
   /// 債券ETF / Bonds ETF.
   f9tws_SymbKind_ETF_B = 0x11,
   /// 槓桿型ETF / Leveraged ETF.
   f9tws_SymbKind_ETF_L = 0x12,
   /// 反向型ETF / Inverse ETF.
   f9tws_SymbKind_ETF_I = 0x13,
   /// 期信託ETF / Futures ETF.
   f9tws_SymbKind_ETF_F = 0x14,

   /// ETF(外幣計價）.
   f9tws_SymbKind_F_ETF = 0x20,
   /// 債券ETF(外幣計價） / Bonds ETF.
   f9tws_SymbKind_F_ETF_B = 0x21,
   /// 槓桿型ETF(外幣計價） / Leveraged ETF.
   f9tws_SymbKind_F_ETF_L = 0x22,
   /// 反向型ETF(外幣計價） / Inverse ETF.
   f9tws_SymbKind_F_ETF_I = 0x23,
   /// 期信託ETF(外幣計價） / Futures ETF.
   f9tws_SymbKind_F_ETF_F = 0x24,

   /// ETN
   f9tws_SymbKind_ETN = 0x30,
   /// 債券型ETN
   f9tws_SymbKind_ETN_B = 0x31,
   /// 槓桿型ETN
   f9tws_SymbKind_ETN_L = 0x32,
   /// 反向型ETN
   f9tws_SymbKind_ETN_I = 0x33,
   /// 期權型ETN
   f9tws_SymbKind_ETN_S = 0x34,

   /// 不動產資產信託受益證券 / REAT / Real Estate Asset Trust.
   f9tws_SymbKind_REAT = 0x40,
   /// 不動產投資信託受益證券 / REIT / Real Estate Investment Trust.
   f9tws_SymbKind_REIT = 0x41,
   /// 金融資產證券化受益證券 / Financial Asset Beneficiary Securities.
   f9tws_SymbKind_FABS = 0x42,

   f9tws_SymbKind_W_BEGIN = 0x50,
   /// 國內標的認購權證 / Call warrant.
   f9tws_SymbKind_WC = 0x50,
   /// 國內標的認售權證 / Put warrant.
   f9tws_SymbKind_WP = 0x51,
   /// 外國標的認購權證 / Foreign call warrant.
   f9tws_SymbKind_WCF = 0x52,
   /// 外國標的認售權證 / Foreign put warrant.
   f9tws_SymbKind_WPF = 0x53,
   /// 下限型認購權證(牛證) / Callable Bull Contracts.
   f9tws_SymbKind_WCBull = 0x54,
   /// 上限型認售權證(熊證) / Callable Bear Contracts.
   f9tws_SymbKind_WCBear = 0x55,
   /// 可展延型下限型認購權證 / Extension Callable Bull Contracts.
   f9tws_SymbKind_WCBullE = 0x56,
   /// 可展延型上限型認售權證 / Extension Callable Bear Contracts.
   f9tws_SymbKind_WCBearE = 0x57,
   f9tws_SymbKind_W_LAST = 0x5f,

   /// 存託憑證(台灣) / TDR / Taiwan Depositary Receipts.
   f9tws_SymbKind_TDR = 0x60,
   /// 存託憑證 可轉換公司債(Convertible bonds)
   f9tws_SymbKind_DR_CB = 0x61,
   /// 存託憑證 附認股權公司債(Corporate bonds with warrants)
   f9tws_SymbKind_DR_CBW = 0x62,
   /// 存託憑證 附認股權公司債_履約後之公司債.
   f9tws_SymbKind_DR_CBWP = 0x63,
   /// 存託憑證 認股權憑證(Stock warrant)
   f9tws_SymbKind_DR_SW = 0x64,

   /// 交換公司債及交換金融債 / Exchangeable corporate bond / Bank debentures.
   f9tws_SymbKind_ECB = 0x70,
   /// 轉換公司債 / Convertible bonds.
   f9tws_SymbKind_CB = 0x71,
   /// 附認股權公司債 / Corporate bonds with warrants.
   f9tws_SymbKind_CBW = 0x72,
   /// 附認股權公司債_履約後之公司債
   f9tws_SymbKind_CBWP = 0x73,
   /// 認股權憑證 / Stock warrant.
   f9tws_SymbKind_SW = 0x74,

   /// 中央公債 / Central Government Securities (CGS).
   /// 地方公債 or 分割公債.
   f9tws_SymbKind_GS = 0xb0,
   /// 金融債券及分割金融債券.
   f9tws_SymbKind_BB = 0xbb,
   /// 外國債券.
   f9tws_SymbKind_BF = 0xbf,
};

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9tws_TwsTypes_h__
