// \file fon9/rc/RcSeedVisitor.h
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitor_h__
#define __fon9_rc_RcSeedVisitor_h__
#include "fon9/rc/RcClientApi.h"

#ifdef __cplusplus
extern "C" {
#endif

/// fon9 SeedVisitor 的基本命名物件.
typedef struct {
   fon9_CStrView  Name_;
   fon9_CStrView  Title_;
   fon9_CStrView  Description_;
   int            Index_;
   char           Padding___[4];
} f9sv_Named;

/// fon9 SeedVisitor 的欄位型別.
typedef struct {
   f9sv_Named  Named_;
   /// 欄位型別.
   /// - Cn = char[n], 若 n==0 則為 Blob.Chars 欄位.
   /// - Bn = byte[n], 若 n==0 則為 Blob.Bytes 欄位.
   /// - Un[.s] = Unsigned, n=bytes(1,2,4,8), s=scale(小數位數)可為0
   /// - Sn[.s] = Signed, n=bytes(1,2,4,8), s=scale(小數位數)可為0
   /// - Unx = Unsigned, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
   /// - Snx = Signed, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
   /// - Ti = TimeInterval
   /// - Ts = TimeStamp
   char  TypeId_[16];
   const void* InternalOwner_;
} f9sv_Field;

//--------------------------------------------------------------------------//

/// 提供一個 table 裡面的資料(使用字串).
/// 通常用在登入成功時, Server 端提供的相關訊息.
/// 例如: OMS使用者的: 可用帳號列表、可用櫃號列表、流量管制參數...
typedef struct {
   const f9sv_Field*       FieldArray_;
   unsigned                FieldCount_;
   unsigned                RecordCount_;
   const fon9_CStrView**   RecordList_;
} f9sv_GvList;

typedef struct {
   f9sv_Named  Named_;
   f9sv_GvList GvList_;
} f9sv_GvTable;

/// 預設的 table 格式:
/// 行的第1個字元, 如果是 *fon9_kCSTR_LEAD_TABLE, 則表示一個 table 的開始.
/// ```
/// fon9_kCSTR_LEAD_TABLE + TableNamed\n
/// FieldNames\n
/// FieldValues\n    可能有 0..n 行
/// ```
/// 可透過 fon9::rc::SvParseGvTables() 解析.
#define fon9_kCSTR_LEAD_TABLE    "\x02"

/// 通常用於: 登入成功後, 收到的相關權限資料(例:可用帳號列表、可用櫃號列表、流量參數...).
/// 以字串形式表示, 格式內容請參閱 fon9_kCSTR_LEAD_TABLE 的說明.
typedef struct {
   fon9_CStrView                 OrigStrView_;
   const f9sv_GvTable* const*    TableList_;
   unsigned                      TableCount_;
   char                          Padding___[4];
} f9sv_GvTables;

//--------------------------------------------------------------------------//

/// - C <- S: SvFuncCode::Acl + ConfigString
typedef struct {
   /// - TableName = PoAcl
   ///   - Fields = Path, Rights, MaxSubrCount
   ///   - Rights = fon9/seed/SeedAcl.hpp: enum class AccessRight;
   f9sv_GvTables  RightsTables_;

   /// 查詢流量管制.
   uint16_t FcQryCount_;
   uint16_t FcQryMS_;
   /// 合計最多可訂閱數量.
   uint32_t MaxSubrCount_;

} f9sv_ClientConfig;
typedef void (fon9_CAPI_CALL *f9sv_FnOnConfig)(f9rc_ClientSession*, const f9sv_ClientConfig* cfg);

/// fon9 seed 機制, 每個 key 可能對應數種資料, 使用 Tab 分類資料,
/// 例如: 某商品Id, 可能有:
/// - Tab="Base": 基本資料(名稱、市場別...)
/// - Tab="Ref":  今日參考價(平盤價、漲停價、跌停價...)
/// - Tab="Deal": 成交價量(成交時間、成交價、成交量...)
/// - Tab="BS":   買賣報價
typedef struct {
   f9sv_Named        Named_;
   const f9sv_Field* FieldArray_;
   unsigned          FieldCount_;
   char              Padding___[4];
} f9sv_Tab;

typedef uint32_t  f9sv_TabSize;

/// Tab 集中起來, 構成一個 Layout, 用來描述一筆完整的資料.
/// - 使用 layout 來描述 pod 的資料格式.
/// - pod 裡面包含多個 seed, 使用 layout 裡面的 tab 來描述一個 seed;
/// - tree 為 pod 的容器: 一棵 tree 包含 0..n 個 pod;
/// - pod 為 seed 的容器: 一個 pod 包含 1..n 個 seed;
/// - seed 可能會有 0..1 棵 sapling(子樹), 是否會有 sapling? 怎樣才有 sapling? 由 seed 決定;
///   - 例如: "台股商品/2330^Deal" = 成交明細資料表.
typedef struct {
   f9sv_Field     KeyField_;
   f9sv_Tab*      TabList_;
   f9sv_TabSize   TabCount_;
   char           Padding___[4];
} f9sv_Layout;

/// 參考 fon9/seed/SeedBase.hpp: enum class OpResult;
/// >=0 表示沒有錯誤, 成功完成.
/// <0 表示錯誤原因.
typedef enum {
   f9sv_Result_NoError = 0,

   /// = fon9::seed::OpResult::access_denied = -2,
   f9sv_Result_AccessDenied = -2,

   /// 在呼叫 f9sv_Query(); 或 f9sv_Subscribe(); 時,
   /// - 沒有呼叫過 f9OmsRc_Initialize();
   /// - 或建立 ses 時, 沒有提供 f9OmsRc_ClientSessionParams 參數.
   f9sv_Result_BadInitParams = -10,

   /// 連線中斷. 查詢、訂閱, 都會收到此回報, 然後清除所有的要求(查詢、訂閱...).
   f9sv_Result_LinkBroken = -11,

   /// 在呼叫 f9sv_Query(); 時, 若有流量管制.
   /// - 先透過 FnOnReport_(f9sv_Result_FlowControl) 告知查詢失敗.
   /// - 然後 f9sv_Query(); 返回>0, 告知應延遲多少 ms;
   f9sv_Result_FlowControl = -12,

   /// f9sv_Query() 正在查詢, 尚未取得結果, 則後續相同的 seedName 直接返回失敗. 
   f9sv_Result_DupQuery = -13,

   /// 已經有訂閱, 則後續的 f9sv_Query()/f9sv_Subscribe() 直接返回失敗.
   f9sv_Result_InSubscribe = -14,
   /// 正在取消訂閱, 必須等取消成功後, 才可再次執行 f9sv_Query()/f9sv_Subscribe(), 直接返回失敗.
   f9sv_Result_InUnsubscribe = -15,

} f9sv_Result;

/// SeedVisitor API 客戶端用戶的事件處理程序及必要參數.
typedef struct {
   f9rc_FunctionNoteParams BaseParams_;

   /// 通知時機:
   /// - 當登入成功後, Server 回覆 tree list.
   /// - 您可以在收到此事件時「訂閱」需要的項目.
   f9sv_FnOnConfig   FnOnConfig_;
} f9sv_ClientSessionParams;

inline void f9sv_InitClientSessionParams(f9rc_ClientSessionParams* f9rcCliParams, f9sv_ClientSessionParams* f9svRcParams) {
   f9rc_InitClientSessionParams(f9rcCliParams, f9svRcParams, f9rc_FunctionCode_SeedVisitor);
}

#ifdef __cplusplus
/// 記錄登入後取得的 Config 訊息.
#define f9sv_ClientLogFlag_Config      static_cast<f9rc_ClientLogFlag>(0x0008)
/// 記錄查詢要求 & 結果(成功 or 失敗, 不含資料內容).
#define f9sv_ClientLogFlag_Query       static_cast<f9rc_ClientLogFlag>(0x0010)
/// 記錄「查詢」的資料內容.
#define f9sv_ClientLogFlag_QueryData   static_cast<f9rc_ClientLogFlag>(0x0020)
/// 記錄訂閱要求 & 結果(成功 or 失敗, 不含資料內容).
#define f9sv_ClientLogFlag_Subscribe   static_cast<f9rc_ClientLogFlag>(0x0040)
/// 記錄「訂閱」的資料內容.
#define f9sv_ClientLogFlag_SubrData    static_cast<f9rc_ClientLogFlag>(0x0080)
#else
#define f9sv_ClientLogFlag_Config      ((f9rc_ClientLogFlag)0x0008)
#define f9sv_ClientLogFlag_Query       ((f9rc_ClientLogFlag)0x0010)
#define f9sv_ClientLogFlag_QueryData   ((f9rc_ClientLogFlag)0x0020)
#define f9sv_ClientLogFlag_Subscribe   ((f9rc_ClientLogFlag)0x0040)
#define f9sv_ClientLogFlag_SubrData    ((f9rc_ClientLogFlag)0x0080)
#endif

typedef struct {
   const char*    TreePath_;
   const char*    SeedKey_;
   /// 若有提供 TabName_ (!= NULL), 則不理會 TabIndex_;
   const char*    TabName_;
   f9sv_TabSize   TabIndex_;
   char           Padding___[4];
} f9sv_SeedName;

/// 收到訂閱或查詢的資料.
typedef struct {
   f9sv_Result    ResultCode_;
   char           Padding___[4];
   /// 使用者訂閱時自訂的編號.
   void*          UserData_;
   /// 查詢或訂閱時提供的 TreePath (已正規化: 移除多餘的 '/', '.' 之類).
   fon9_CStrView  TreePath_;
   /// 不論 Server 端的 key 為何種型別, 回報事件都會提供字串.
   fon9_CStrView  SeedKey_;
   /// TreePath 的資料格式.
   const f9sv_Layout* Layout_;
   /// 此次回報的是哪個 Tab 的資料.
   const f9sv_Tab*    Tab_;
   /// 可透過 f9sv_GetField_*() 取得欄位內容.
   const struct f9sv_Seed* Seed_;
} f9sv_ClientReport;
typedef void (fon9_CAPI_CALL *f9sv_FnOnReport)(f9rc_ClientSession*, const f9sv_ClientReport* rpt);

typedef struct {
   /// 通知收到: 查詢結果、訂閱結果、訂閱訊息...
   f9sv_FnOnReport   FnOnReport_;
   void*             UserData_;
} f9sv_ReportHandler;

//--------------------------------------------------------------------------//

#define f9sv_CAPI_FN    fon9_CAPI_FN

/// 啟動 f9sv(fon9 SeedVisitor Rc client) 函式庫.
/// - 請使用時注意: 禁止 multi thread 同時呼叫 f9sv_Initialize()/fon9_Finalize();
/// - 可重覆呼叫 f9sv_Initialize(), 但須有對應的 fon9_Finalize();
///   最後一個 fon9_Finalize() 被呼叫時, 結束函式庫.
/// - 其餘請參考 fon9_Initialize();
/// - 然後透過 f9rc_CreateClientSession() 建立 Session;
///   - 建立 Session 時, 必須提供 f9sv_ClientSessionParams; 參數.
///   - 且需要經過 f9sv_InitClientSessionParams() 初始化.
f9sv_CAPI_FN(int) f9sv_Initialize(const char* logFileFmt);

/// 送出查詢訊息.
/// 查詢結果不一定會依照查詢順序回報.
/// \retval =f9sv_Result_NoError 查詢要求已送出(或因尚未取得 Layout 而暫時保留).
///            如果網路查詢返回夠快, 返回前可能已透過 FnOnReport_() 通知查詢結果(可能成功 or 失敗).
/// \retval <f9sv_Result_NoError 無法查詢, 失敗原因請參考 f9sv_Result;
///            會先用 FnOnReport_() 通知查詢失敗, 然後才返回.
/// \retval >f9sv_Result_NoError 流量管制, 需要等 ((unsigned)retval) ms 之後才能再查詢.
///            會先用 FnOnReport_() 通知查詢失敗, 然後才返回.
f9sv_CAPI_FN(f9sv_Result)
f9sv_Query(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler);

/// 訂閱要求.
/// \retval =f9sv_Result_NoError 訂閱要求已送出.
///            如果網路查詢返回夠快, 返回前可能已透過 FnOnReport_() 通知訂閱結果及訊息.
/// \retval <f9sv_Result_NoError 無法訂閱, 失敗原因請參考 f9sv_Result;
///            會先用 FnOnReport_() 通知訂閱失敗, 然後才返回.
f9sv_CAPI_FN(f9sv_Result)
f9sv_Subscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler);

/// 取消訂閱.
/// 返回後就不再有訂閱的事件通知, 但網路上可能仍有訂閱的訊息正在傳送, 收到後會拋棄.
/// 等收到 Server 端確認取消訂閱時, 另有事件通知.
f9sv_CAPI_FN(void)
f9sv_Unsubscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName);

//--------------------------------------------------------------------------//

/// 將欄位轉成文字, 尾端填入 EOS, 若緩衝區不足, 則僅顯示部分內容.
/// *bufsz 傳回包含 EOS 需要的資料量.
f9sv_CAPI_FN(const char*)
f9sv_GetField_Str(const struct f9sv_Seed* seed, const f9sv_Field* fld, char* outbuf, unsigned* bufsz);

inline const char*
f9sv_GetField_StrN(const struct f9sv_Seed* seed, const f9sv_Field* fld, char* outbuf, unsigned bufsz) {
   return f9sv_GetField_Str(seed, fld, outbuf, &bufsz);
}

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__fon9_rc_RcSeedVisitor_h__
