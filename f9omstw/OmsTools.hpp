// \file f9omstw/OmsTools.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsTools_hpp__
#define __f9omstw_OmsTools_hpp__
#include "fon9/StrView.hpp"

namespace f9omstw {

/// 「數字字串」 + 1;
/// 返回 true 表示成功, false = overflow;
typedef bool (*FnIncStr)(char* pbeg, char* pend);
bool IncStrDec(char* pbeg, char* pend);
bool IncStrAlpha(char* pbeg, char* pend);

/// Container = fon9::SortedVectorSet<smart_pointer>;
/// 可用 fon9::StrView keyText; 尋找元素.
template <class Container>
typename Container::value_type ContainerRemove(Container& container, fon9::StrView keyText) {
   auto ifind = container.find(keyText);
   if (ifind == container.end())
      return typename Container::value_type{};
   typename Container::value_type retval = std::move(*ifind);
   container.erase(ifind);
   return retval;
}

} // namespaces
#endif//__f9omstw_OmsTools_hpp__
