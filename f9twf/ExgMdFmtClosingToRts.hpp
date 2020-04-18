// \file f9twf/ExgMdFmtClosingToRts.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtClosingToRts_hpp__
#define __f9twf_ExgMdFmtClosingToRts_hpp__
#include "f9twf/ExgMcChannel.hpp"

namespace f9twf {

/// 解析 static_cast<const ExgMcI071*>(&e.Pk_); 結算價.
/// 並透過 symb.MdRtStream_ 發行.
f9twf_API void I071ClosingParserToRts(ExgMcMessage& e);

/// 解析 static_cast<const ExgMcI072*>(&e.Pk_); 結算價.
/// 並透過 symb.MdRtStream_ 發行.
f9twf_API void I072ClosingParserToRts(ExgMcMessage& e);

} // namespaces
#endif//__f9twf_ExgMdFmtClosingToRts_hpp__
