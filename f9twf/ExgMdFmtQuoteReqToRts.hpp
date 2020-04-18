// \file f9twf/ExgMdFmtQuoteReqToRts.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtQuoteReqToRts_hpp__
#define __f9twf_ExgMdFmtQuoteReqToRts_hpp__
#include "f9twf/ExgMcChannel.hpp"

namespace f9twf {

/// 解析 static_cast<const ExgMcI100*>(&e.Pk_); 詢價訊息.
/// 並透過 symb.MdRtStream_ 發行.
f9twf_API void I100QuoteReqParserToRts(ExgMcMessage& e);

} // namespaces
#endif//__f9twf_ExgMdFmtQuoteReqToRts_hpp__
