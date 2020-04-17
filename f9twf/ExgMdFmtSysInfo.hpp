// \file f9twf/ExgMdFmtSysInfo.hpp
//
// 台灣期交所行情格式: 系統訊息 I140;
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtSysInfo_hpp__
#define __f9twf_ExgMdFmtSysInfo_hpp__
#include "f9twf/ExgMdFmt.hpp"
#include "fon9/fmkt/SymbDynBand.hpp"
#include "fon9/fmkt/SymbBreakSt.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdSysInfo {
   fon9::PackBcd<4>  FunctionCode_;
};

enum class ExgMdSysInfo_ListType : uint8_t {
   All = 0x00,
   FlowGroup = 0x01,
   Contract = 0x02,
   Symbol = 0x03,
   OptYYYYMM = 0x04,
};

using ExgMdSysInfo_ContractId = fon9::CharAryF<3>;
union ExgMdSysInfo_Id {
   /// ListType_ == ExgMdSysInfo_ListType::Contract == 2;
   ExgMdSysInfo_ContractId ContractId3_;
   /// ListType_ == ExgMdSysInfo_ListType::Symbol == 3;
   ExgMdProdId10           ProdId10_;
};
struct ExgMdSysInfo_IdList {
   fon9::PackBcd<2>  Count_;
   ExgMdSysInfo_Id   List_[1];
};
union ExgMdSysInfo_StList {
   /// ListType_ == ExgMdSysInfo_ListType::FlowGroup == 1;
   fon9::PackBcd<2>     FlowGroup_;
   ExgMdSysInfo_IdList  IdList_;
};
//-////////////////////////////////////////////////////////////////////////-//
enum class ExgMdSysInfo_ExpandType : uint8_t {
   /// 僅放寬漲幅階段.
   UpLmt = 0x01,
   /// 僅放寬跌幅階段
   DnLmt = 0x02,
   /// 同時放寬漲幅及跌幅階段.
   UDLmt = 0x03,
};
fon9_ENABLE_ENUM_BITWISE_OP(ExgMdSysInfo_ExpandType);

/// FUNCTION-CODE = 100: 契約或商品觸及漲跌停價而「即將」放寬漲跌幅時以此訊息通知。
struct ExgMdSysInfo10x : public ExgMdSysInfo {
   ExgMdSysInfo_ListType   ListType_;
   fon9::PackBcd<2>        Level_;
   ExgMdSysInfo_ExpandType ExpandType_;
   ExgMdSysInfo_IdList     IdList_;
};
/// FUNCTION-CODE = 101: 契約或商品放寬漲跌幅至某一階段時以此訊息通知。
using ExgMdSysInfo100 = ExgMdSysInfo10x;
using ExgMdSysInfo101 = ExgMdSysInfo10x;
//-////////////////////////////////////////////////////////////////////////-//
/// FUNCTION-CODE = 200: 發布暫停交易契約.
struct ExgMdSysInfo200 : public ExgMdSysInfo {
   ExgMdSysInfo_ListType   ListType_;
   f9fmkt_BreakReason      Reason_;
   fon9::PackBcd<6>        BreakHHMMSS_;
   ExgMdSysInfo_IdList     IdList_;
};
/// FUNCTION-CODE = 201: 發布恢復交易契約.
struct ExgMdSysInfo201 : public ExgMdSysInfo {
   ExgMdSysInfo_ListType   ListType_;
   f9fmkt_BreakReason      Reason_;
   /// 開始收單時間.
   fon9::PackBcd<6>        StartHHMMSS_;
   /// 重新開盤時間.
   fon9::PackBcd<6>        ReopenHHMMSS_;
   ExgMdSysInfo_IdList     IdList_;
};
//-////////////////////////////////////////////////////////////////////////-//
/// FUNCTION-CODE = 302: 開始收單.
/// FUNCTION-CODE = 304: 開盤.
/// FUNCTION-CODE = 305: 不可刪單階段(Non-cancel Period)
/// FUNCTION-CODE = 306: 收盤/或此商品不再恢復交易.
struct ExgMdSysInfo30x : public ExgMdSysInfo {
   ExgMdSysInfo_ListType   ListType_;
   f9fmkt_BreakReason      Reason_;
   ExgMdSysInfo_StList     List_;
};
using ExgMdSysInfo302 = ExgMdSysInfo30x;
using ExgMdSysInfo304 = ExgMdSysInfo30x;
using ExgMdSysInfo305 = ExgMdSysInfo30x;
using ExgMdSysInfo306 = ExgMdSysInfo30x;
//-////////////////////////////////////////////////////////////////////////-//
using ExgMdSysInfo_OptYM = fon9::CharAry<9>;

union ExgMdSysInfo_IdL {
   /// ListType_ == ExgMdSysInfo_ListType::Contract == 2;
   fon9::CharAryF<3>    ContractId3_;
   /// ListType_ == ExgMdSysInfo_ListType::Symbol == 3;
   ExgMdProdId20        ProdId20_;
   /// ListType_ == ExgMdSysInfo_ListType::OptYYYYMM == 4;
   ExgMdSysInfo_OptYM   OptYYYYMM_;
};
struct ExgMdSysInfo_IdListL {
   fon9::PackBcd<2>  Count_;
   ExgMdSysInfo_IdL  List_[1];
};
/// FUNCTION-CODE = 400: 發布「暫停」動態價格穩定時以此訊息通知。
/// FUNCTION-CODE = 403: 預告「暫停」.
/// FUNCTION-CODE = 401: 發布「恢復」。
/// FUNCTION-CODE = 404: 預告「恢復」.
struct ExgMdSysInfo40x : public ExgMdSysInfo {
   ExgMdSysInfo_ListType      ListType_;
   f9fmkt_DynBandStopReason   Reason_;
   fon9::PackBcd<6>           HHMMSS_;
   ExgMdSysInfo_IdListL       List_;
};
using ExgMdSysInfo400 = ExgMdSysInfo40x;
using ExgMdSysInfo403 = ExgMdSysInfo40x;
using ExgMdSysInfo401 = ExgMdSysInfo40x;
using ExgMdSysInfo404 = ExgMdSysInfo40x;

/// FUNCTION-CODE = 402: 發布動態價格穩定「調整範圍」時以此訊息通知。
/// FUNCTION-CODE = 405: 預告.
struct ExgMdSysInfo40r : public ExgMdSysInfo {
   ExgMdSysInfo_ListType   ListType_;
   fon9::PackBcd<6>        HHMMSS_;
   fon9::PackBcd<2>        Range9V9_;
   f9fmkt_DynBandSide      SideType_;
   ExgMdSysInfo_IdListL    List_;
};
using ExgMdSysInfo402 = ExgMdSysInfo40r;
using ExgMdSysInfo405 = ExgMdSysInfo40r;
//-////////////////////////////////////////////////////////////////////////-//
/// 逐筆行情 系統訊息: Fut:(Tx='2'; Mg='3'); Opt:(Tx='5'; Mg='3'); Ver=6;
struct ExgMcI140 : public ExgMcHead
                 , public ExgMdSysInfo {
};
//--------------------------------------------------------------------------//
/// 間隔行情 系統訊息: Fut:(Tx='2'; Mg='3'); Opt:(Tx='5'; Mg='3'); Ver=6;
struct ExgMiI140 : public ExgMiHead
                 , public ExgMdSysInfo {
};
//--------------------------------------------------------------------------//
template <class T, class I140T>
inline const T& I140CastTo(const I140T& i140) {
   return *static_cast<const T*>(static_cast<const ExgMdSysInfo*>(&i140));
}

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtSysInfo_hpp__
