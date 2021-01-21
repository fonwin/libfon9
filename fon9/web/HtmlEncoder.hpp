// \file fon9/web/HtmlEncoder.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_HtmlEncoder_hpp__
#define __fon9_web_HtmlEncoder_hpp__
#include "fon9/buffer/RevBuffer.hpp"

namespace fon9 { namespace web {

/// 將 str 的特殊字元(例: '<', '>', '&') 轉換成 HTML 編碼(例: "&lt;", "&gt;", "&amp;").
fon9_API void RevEncodeHtml(RevBuffer& rbuf, StrView str);

} } // namespaces
#endif//__fon9_web_HtmlEncoder_hpp__
