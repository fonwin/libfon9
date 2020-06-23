// \file fon9/seed/FieldType.h
// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldType_h__
#define __fon9_seed_FieldType_h__
#include "fon9/rc/RcClientApi.h"
#include "fon9/Assert.h"

#ifdef __cplusplus
extern "C" {
#endif

/// \ingroup seed
/// 欄位儲存的資料類型.
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
   /// >= f9sv_FieldType_Integer 之後皆為數字型別.
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
inline int f9sv_IsFieldTypeNumber(f9sv_FieldType v) {
   return v >= f9sv_FieldType_Integer;
}

/// 小數位數.
typedef uint8_t   f9sv_DecScale;

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__fon9_seed_FieldType_h__
