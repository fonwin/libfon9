// \file f9twf/ExgMdFmtBSToRts.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtBSToRts_hpp__
#define __f9twf_ExgMdFmtBSToRts_hpp__
#include "f9twf/ExgMcChannel.hpp"

namespace f9twf {

/// 解析 static_cast<const ExgMcI081*>(&e.Pk_);
/// 並透過 symb.MdRtStream_ 發行.
f9twf_API void I081BSParserToRts(ExgMcMessage& e);

/// 解析 static_cast<const ExgMcI083*>(&e.Pk_);
/// 並透過 symb.MdRtStream_ 發行.
f9twf_API void I083BSParserToRts(ExgMcMessage& e);

} // namespaces
#endif//__f9twf_ExgMdFmtBSToRts_hpp__
