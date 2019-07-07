// \file fon9/rc/Rc.h
// \author fonwinz@gmail.com
#ifndef __fon9_rc_Rc_h__
#define __fon9_rc_Rc_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

fon9_ENUM(f9rc_RcFlag, uint16_t) {
   f9rc_RcFlag_NoChecksum = 0x01,
};

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__fon9_rc_Rc_h__
