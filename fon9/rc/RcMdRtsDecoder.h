// \file fon9/rc/RcMdRtsDecoder.h
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcMdRtsDecoder_h__
#define __fon9_rc_RcMdRtsDecoder_h__
#include "fon9/rc/RcSeedVisitor.h"
#include "fon9/fmkt/FmdTypes.h"
#include "fon9/fmkt/FmdRtsPackType.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus
   
   /// 將 "MdRts" 加入 StreamDecoder; 之後可以使用: "$TabName:MdRts:Args"; 的格式.
   /// - 在 f9sv_Initialize() 之後, f9sv_Subscribe() 之前, 執行一次即可.
   /// - 行情訂閱: 訂閱參數=? 如何回補? 如何區分日夜盤?
   ///   回補應可到「前一盤」, 不論前一盤是日盤或夜盤.
   f9sv_CAPI_FN(f9sv_Result) f9sv_AddStreamDecoder_MdRts(void);

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__fon9_rc_RcMdRtsDecoder_h__
