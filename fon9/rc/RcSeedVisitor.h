// \file fon9/rc/RcSeedVisitor.h
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitor_h__
#define __fon9_rc_RcSeedVisitor_h__
#include "fon9/rc/RcClientApi.h"
#include "fon9/Assert.h"

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

/// 欄位型別.
fon9_ENUM(f9sv_FieldType, uint8_t) {
   /// 類型不明, 或自訂欄位.
   f9sv_FieldType_Unknown,
   /// bytes 陣列.
   /// 顯示字串 = 需要額外轉換(例: HEX, Base64...)
   f9sv_FieldType_Bytes,
   /// 字元串(尾端不一定會有EOS).
   /// 顯示字串 = 直接使用內含字元, 直到EOS or 最大長度.
   f9sv_FieldType_Chars,
   /// 整數欄位.
   f9sv_FieldType_Integer = 10,
   /// 固定小數位欄位: fon9::Decimal
   f9sv_FieldType_Decimal,
   /// 時間戳.
   f9sv_FieldType_TimeStamp,
   /// 時間間隔.
   f9sv_FieldType_TimeInterval,
   /// 時間: 距離 00:00:00 的時間間隔.
   /// 輸出格式 `days-hh:mm:ss.uuuuuu`
   f9sv_FieldType_DayTime,
};

/// 小數位數.
typedef uint8_t   f9sv_DecScale;

/// fon9 SeedVisitor 的欄位型別.
typedef struct {
   f9sv_Named  Named_;
   const void* InternalOwner_;
   /// 欄位型別.
   /// - Cn = char[n], 若 n==0 則為 Blob.Chars 欄位.
   /// - Bn = byte[n], 若 n==0 則為 Blob.Bytes 欄位.
   /// - Un[.s] = Unsigned, n=bytes(1,2,4,8), s=scale(小數位數)可為0
   /// - Sn[.s] = Signed, n=bytes(1,2,4,8), s=scale(小數位數)可為0
   /// - Unx = Unsigned, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
   /// - Snx = Signed, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
   /// - Ti = TimeInterval
   /// - Ts = TimeStamp
   /// - Td = DayTime
   char           TypeId_[22];
   f9sv_DecScale  DecScale_;
   f9sv_FieldType Type_;
   /// 欄位資料的大小.
   uint32_t       Size_;
   /// ((const char*)(seed) + Offset_) = 儲存原始資料的位置.
   /// 如果您確定要取得的是整數型內容, 可藉由此方式快速取得, 但請注意記憶體對齊問題.
   int32_t        Offset_;
   /// 若為數值類型的欄位,
   /// 透過 f9sv_GetField_sNN() 或 f9sv_GetField_uNN() 取得的數字, 若與這裡相同, 則表示該值為 null;
   /// - 若為 f9sv_FieldType_Decimal:
   ///   - Unsigned_ = max uint64_t or uint32_t or uint16_t or uint8_t;
   ///   - Signed_   = min int64_t or int32_t or int16_t or int8_t;
   /// - 若為整數, 則為 0;
   union {
      uint64_t    Unsigned_;
      int64_t     Signed_;
   } NullValue_;
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
   f9sv_Tab*      TabArray_;
   f9sv_TabSize   TabCount_;
   char           Padding___[4];
} f9sv_Layout;

/// 參考 fon9/seed/SeedBase.hpp: enum class OpResult;
/// >=0 表示沒有錯誤.
/// <0 表示錯誤原因. -10 .. -30 保留給 f9sv_Result;
typedef enum {
   f9sv_Result_NoError = 0,

   /// 呼叫 f9sv_Unsubscribe() 時, 通知 regHandler 訂閱取消中.
   /// 告知 regHandler: 後續的訂閱資料及取消結果由 unregHandler 接手.
   f9sv_Result_Unsubscribing = 9641,
   /// f9sv_Unsubscribe() 成功的通知.
   f9sv_Result_Unsubscribed = 9645,

   /// 訂閱 tree, 收到 NotifyKind = PodRemoved 通知.
   /// 此時應移除該 pod.
   f9sv_Result_SubrNotifyPodRemoved = 9652,
   /// 訂閱 tree, 收到 NotifyKind = SeedRemoved 通知.
   /// 此時應移除該 seed.
   f9sv_Result_SubrNotifySeedRemoved = 9653,
   /// 收到 NotifyKind = TableChanged 時.
   /// 此時應清除訂閱的資料表內容, 通常會立即回覆新的資料表.
   f9sv_Result_SubrNotifyTableChanged = 9654,
   /// - 收到 NotifyKind = ParentSeedClear 通知.
   /// - 訂閱 seed, 收到 NotifyKind = PodRemoved 或 SeedRemoved 通知.
   f9sv_Result_SubrNotifyUnsubscribed = 9655,

   /// 收到 NotifyKind = SubscribeStreamOK 通知.
   f9sv_Result_SubrStreamOK = 9656,
   /// 收到 NotifyKind = StreamRecover 通知.
   /// - 一般訂閱(SeedChanged) 及 Stream訂閱(StreamData),
   ///   回報時使用 f9sv_Result_NoError;
   f9sv_Result_SubrStreamRecover = 9658,
   /// 收到 NotifyKind = StreamRecoverEnd 通知.
   /// 如果回補完後, Stream 已確定不會再有訊息,
   /// 則會直接回覆 StreamEnd, 不會有 StreamRecoverEnd;
   f9sv_Result_SubrStreamRecoverEnd = 9659,
   /// 收到 NotifyKind = StreamEnd 通知.
   /// 收到後會自動移除訂閱, 不會再收到該 Stream 的訊息.
   f9sv_Result_SubrStreamEnd = 9660,

   /// = fon9::seed::OpResult::access_denied = -2,
   f9sv_Result_AccessDenied = -2,
   /// = fon9::seed::OpResult::not_found_tab = -201,
   f9sv_Result_NotFoundTab = -201,

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
   f9sv_Result_InQuery = -13,

   /// 超過可訂閱數量.
   /// 在收到 Server 端的「取消訂閱確認」之後, 才會扣除訂閱數量.
   /// 目前: 僅支援「Session累計訂閱數量」限制, 沒有檢查「個別 tree 的訂閱數量」.
   f9sv_Result_OverMaxSubrCount = -20,

   /// 已經有訂閱, 則後續的 f9sv_Query()/f9sv_Subscribe() 直接返回失敗.
   f9sv_Result_InSubscribe = -21,
   /// 正在取消訂閱, 必須等取消成功後, 才可再次執行 f9sv_Query()/f9sv_Subscribe(), 直接返回失敗.
   /// 或 Server 執行訂閱處理時, 收到取消.
   f9sv_Result_InUnsubscribe = -22,

} f9sv_Result;

/// SeedVisitor API 客戶端用戶的事件處理程序及必要參數.
typedef struct {
   f9rc_FunctionNoteParams BaseParams_;

   /// 通知時機:
   /// - 當登入成功後, Server 回覆 tree list.
   /// - 您可以在收到此事件時「訂閱」需要的項目.
   f9sv_FnOnConfig   FnOnConfig_;
} f9sv_ClientSessionParams;

static inline void f9sv_InitClientSessionParams(f9rc_ClientSessionParams* f9rcCliParams, f9sv_ClientSessionParams* f9svRcParams) {
   f9rc_InitClientSessionParams(f9rcCliParams, f9svRcParams, f9rc_FunctionCode_SeedVisitor);
}

#ifdef __cplusplus
/// 記錄登入後取得的 Config 訊息.
#define f9sv_ClientLogFlag_Config         static_cast<f9rc_ClientLogFlag>(0x0008)
/// 記錄查詢要求 & 結果(成功 or 失敗, 不含資料內容).
#define f9sv_ClientLogFlag_Query          static_cast<f9rc_ClientLogFlag>(0x0010)
/// 記錄「查詢」的資料內容.
#define f9sv_ClientLogFlag_QueryData      static_cast<f9rc_ClientLogFlag>(0x0020)
/// 記錄訂閱要求 & 結果(成功 or 失敗, 不含資料內容).
#define f9sv_ClientLogFlag_Subscribe      static_cast<f9rc_ClientLogFlag>(0x0040)
/// 記錄「訂閱」的資料內容.
#define f9sv_ClientLogFlag_SubscribeData  static_cast<f9rc_ClientLogFlag>(0x0080)
#else
#define f9sv_ClientLogFlag_Config         ((f9rc_ClientLogFlag)0x0008)
#define f9sv_ClientLogFlag_Query          ((f9rc_ClientLogFlag)0x0010)
#define f9sv_ClientLogFlag_QueryData      ((f9rc_ClientLogFlag)0x0020)
#define f9sv_ClientLogFlag_Subscribe      ((f9rc_ClientLogFlag)0x0040)
#define f9sv_ClientLogFlag_SubscribeData  ((f9rc_ClientLogFlag)0x0080)
#endif

typedef struct {
   const char*    TreePath_;
   const char*    SeedKey_;
   /// 若有提供 TabName_ (!= NULL), 則不理會 TabIndex_;
   /// - 查詢可使用 "*" 表示查詢全部的 tabs.
   /// - 訂閱、取消訂閱不支援 "*".
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

/// 通知收到 SeedVisitor 的: 查詢結果、訂閱結果、訂閱訊息...
/// - 在通知前會鎖定 session, 您應該盡快結束事件處理, 且不建議在此等候其他 thread.
/// - 尤其不能等候其他 thread 處理「f9sv_ API 呼叫」, 這樣會造成死結.
typedef void (fon9_CAPI_CALL *f9sv_FnOnReport)(f9rc_ClientSession* session, const f9sv_ClientReport* rpt);

typedef struct {
   f9sv_FnOnReport   FnOnReport_;
   void*             UserData_;
} f9sv_ReportHandler;

/// 訂閱代號, 用於 Client <-> Server 之間的通訊.
/// 從 0 開始依序編號, 取消訂閱後此編號會回收再用, 先使用回收的編號, 回收的用完後, 才會增加新的編號.
typedef uint32_t  f9sv_SubrIndex;

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
/// \retval ==f9sv_Result_NoError 查詢要求已送出(或因尚未取得 Layout 而暫時保留).
///            如果網路查詢返回夠快, 返回前可能已透過 FnOnReport_() 通知查詢結果(可能成功 or 失敗).
/// \retval <f9sv_Result_NoError 無法查詢, 失敗原因請參考 f9sv_Result;
///            會先用 FnOnReport_() 通知查詢失敗, 然後才返回.
/// \retval >f9sv_Result_NoError 流量管制, 需要等 ((unsigned)retval) ms 之後才能再查詢.
///            會先用 FnOnReport_() 通知查詢失敗, 然後才返回.
f9sv_CAPI_FN(f9sv_Result)
f9sv_Query(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler);

/// 訂閱要求.
/// 訂閱通知流程:
/// - f9sv_Subscribe(): rpt.Seed_ == NULL;
///   - 成功: rpt.ResultCode_ == f9sv_Result_NoError;
///   - 失敗: rpt.ResultCode_ < f9sv_Result_NoError;
/// - 收到訂閱的資料:
///   - rpt.Seed_ != NULL;
///   - rpt.ResultCode_ == f9sv_Result_NoError;
/// - f9sv_Unsubscribe(): rpt.Seed_ == NULL;
///   - 使用訂閱時的 regHandler 通知: rpt.ResultCode_ == f9sv_Result_Unsubscribing;
///   - 之後不會再使用訂閱時的 regHandler 通知, 改成使用「取消訂閱時的 unregHandler」通知.
///   - 若在取消訂閱成功之前, 還有「收到訂閱的資料」, 則會使用「取消訂閱時的 unregHandler」通知.
/// - 訂閱 Stream 訊息時, 必須用 seedName->TabName_ = "$TabName:StreamDecoderName:Args"; 的格式.
///
/// \retval >=f9sv_Result_NoError, 訂閱要求已送出.
///            如果網路返回夠快, 返回前可能已透過 FnOnReport_() 通知訂閱結果及訊息.
/// \retval <f9sv_Result_NoError 無法訂閱, 失敗原因請參考 f9sv_Result;
///            會先用 FnOnReport_() 通知訂閱失敗, 然後才返回.
f9sv_CAPI_FN(f9sv_Result)
f9sv_Subscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler regHandler);

/// 取消訂閱.
/// 若在取消訂閱成功之前, 還有「收到訂閱的資料」, 則會使用「取消訂閱時的 unregHandler」通知.
///
/// \retval ==f9sv_Result_NoError 取消訂閱要求已送出.
///            - 返回前會先使用「訂閱時的 regHandler」通知 f9sv_Result_Unsubscribing;
///            - 如果網路返回夠快, 返回前可能已透過 unregHandler 通知「訂閱已取消 f9sv_Result_Unsubscribed」.
/// \retval ==f9sv_Result_Unsubscribed 已取消「尚未送出的訂閱要求」.
///            - 返回前會先使用「訂閱時的 regHandler」通知 f9sv_Result_Unsubscribing;
///            - 返回前已透過 unregHandler 通知「訂閱已取消 f9sv_Result_Unsubscribed」.
/// \retval <f9sv_Result_NoError  無法取消訂閱, 失敗原因請參考 f9sv_Result;
///            會先用 unregHandler 通知取消失敗, 然後才返回.
f9sv_CAPI_FN(f9sv_Result)
f9sv_Unsubscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler unregHandler);

/// 取得 res 的文字說明.
f9sv_CAPI_FN(const char*)
f9sv_GetSvResultMessage(f9sv_Result res);

//--------------------------------------------------------------------------//

/// 將欄位轉成文字, 尾端填入 EOS, 若緩衝區不足, 則僅顯示部分內容.
/// *bufsz 傳回包含 EOS 需要的資料量.
/// 返回 outbuf;
f9sv_CAPI_FN(const char*)
f9sv_GetField_Str(const struct f9sv_Seed* seed, const f9sv_Field* fld, char* outbuf, unsigned* bufsz);

static inline const char*
f9sv_GetField_StrN(const struct f9sv_Seed* seed, const f9sv_Field* fld, char* outbuf, unsigned bufsz) {
   return f9sv_GetField_Str(seed, fld, outbuf, &bufsz);
}

#ifdef __cplusplus
#define fon9_OFFSET_CAST(type, src, ofs)  reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(src) + ofs)
#else
#define fon9_OFFSET_CAST(type, src, ofs)  (type*)((uintptr_t)(src) + ofs)
#endif

#define f9sv_GET_FIELD_1_FN(fnName, type) \
static inline type fnName(const struct f9sv_Seed* seed, const f9sv_Field* fld) { \
   assert(fld->Size_ == sizeof(type) && sizeof(type) == 1); \
   return *fon9_OFFSET_CAST(type, seed, fld->Offset_); \
}
f9sv_GET_FIELD_1_FN(f9sv_GetField_u8, uint8_t)
f9sv_GET_FIELD_1_FN(f9sv_GetField_s8, int8_t)
f9sv_GET_FIELD_1_FN(f9sv_GetField_char, char)

#define f9sv_GET_FIELD_INT_FN(fnName, type) \
static inline type fnName(const struct f9sv_Seed* seed, const f9sv_Field* fld) { \
   assert(fld->Size_ == sizeof(type)); \
   type val; \
   memcpy(&val, fon9_OFFSET_CAST(type, seed, fld->Offset_), sizeof(val)); \
   return val; \
}
f9sv_GET_FIELD_INT_FN(f9sv_GetField_u16, uint16_t)
f9sv_GET_FIELD_INT_FN(f9sv_GetField_u32, uint32_t)
f9sv_GET_FIELD_INT_FN(f9sv_GetField_u64, uint64_t)
f9sv_GET_FIELD_INT_FN(f9sv_GetField_s16, int16_t)
f9sv_GET_FIELD_INT_FN(f9sv_GetField_s32, int32_t)
f9sv_GET_FIELD_INT_FN(f9sv_GetField_s64, int64_t)

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__fon9_rc_RcSeedVisitor_h__
