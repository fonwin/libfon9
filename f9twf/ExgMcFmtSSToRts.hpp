// \file f9twf/ExgMcFmtSSToRts.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcFmtSSToRts_hpp__
#define __f9twf_ExgMcFmtSSToRts_hpp__
#include "f9twf/ExgMcChannel.hpp"

namespace f9twf {

/// 解析 static_cast<const ExgMcI084*>(&e.Pk_);
/// - 透過 symb.MdRtStream_ 發行 & 儲存.
/// - 轉發 I083 給 RealtimeChannel;
/// - 若沒有儲存, 則盤中重啟後, Rti 儲存的「BS明細」,
///   可能無法正確處理接續的 UpdateBS;
f9twf_API void I084SSParserToRts(ExgMcMessage& e);
f9twf_API void I084SSParserToRtsV3(ExgMcMessage& e);

} // namespaces
#endif//__f9twf_ExgMcFmtSSToRts_hpp__
