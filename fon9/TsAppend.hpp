// \file fon9/TsAppend.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_TsAppend_hpp__
#define __fon9_TsAppend_hpp__
#include "fon9/FileAppender.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 {

/// 將 pkptr 加入 pklog 尾端, 格式如下:
/// - 0xe0 (fon9_BitvT_TimeStamp_Orig7)
/// - 收到封包的時間: EpochTime(UTC).us: 7 bytes, BigEndian
/// - 封包大小 pksz: 2 bytes, BigEndian
/// - 封包內容 pkptr;
fon9_API void TsAppend(AsyncFileAppender& pklog, TimeStamp now,
                       const void* pkptr, uint16_t pksz);

} // namespaces
#endif//__fon9_TsAppend_hpp__
