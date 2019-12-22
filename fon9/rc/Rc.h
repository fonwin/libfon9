// \file fon9/rc/Rc.h
// \author fonwinz@gmail.com
#ifndef __fon9_rc_Rc_h__
#define __fon9_rc_Rc_h__
#include "fon9/sys/Config.h"
#include "fon9/CStrView.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

fon9_ENUM(f9rc_RcFlag, uint16_t) {
   f9rc_RcFlag_NoChecksum = 0x01,
};

fon9_ENUM(f9rc_FunctionCode, uint8_t) {
   f9rc_FunctionCode_Connection = 0,
   f9rc_FunctionCode_SASL = 1,
   f9rc_FunctionCode_Logout = 2,
   f9rc_FunctionCode_Heartbeat = 3,

   f9rc_FunctionCode_SeedVisitor = 6,
   f9rc_FunctionCode_OmsApi = 7,

   /// 這裡提供一個暫時的最大值, 可減少一些記憶體用量.
   /// 目前規劃:
   /// - admin 功能(登入、Heartbeat...)
   /// - SeedVisitor
   /// - OmsRc
   /// - 其他請參考 fon9/rc/README.md
   f9rc_FunctionCode_Count = 8,
};

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__fon9_rc_Rc_h__
