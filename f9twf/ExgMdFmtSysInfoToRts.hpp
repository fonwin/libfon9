// \file f9twf/ExgMdFmtSysInfoToRts.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtSysInfoToRts_hpp__
#define __f9twf_ExgMdFmtSysInfoToRts_hpp__
#include "f9twf/ExgMcChannel.hpp"

namespace f9twf {

/// 解析 static_cast<const ExgMcI140*>(&e.Pk_);
/// 並透過 symb.MdRtStream_ 發行.
f9twf_API void I140SysInfoParserToRts(ExgMcMessage& e);

} // namespaces
#endif//__f9twf_ExgMdFmtSysInfoToRts_hpp__
