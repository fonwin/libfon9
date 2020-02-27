// \file f9twf/ExgMdFmtBasicInfoToRts.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtBasicInfoToRts_hpp__
#define __f9twf_ExgMdFmtBasicInfoToRts_hpp__
#include "f9twf/ExgMcChannel.hpp"

namespace f9twf {

/// 解析 static_cast<const ExgMcI010*>(&e.Pk_);
/// 並透過 symb.MdRtStream_ 發行.
f9twf_API void I010BasicInfoParserToRts(ExgMcMessage& e);

} // namespaces
#endif//__f9twf_ExgMdFmtBasicInfoToRts_hpp__
