// \file fon9/SimpleFactory.cpp
/// \author fonwinz@gmail.com
#include "fon9/SimpleFactory.hpp"

namespace fon9 {

fon9_API void SimpleFactoryPrintDupName(StrView name, unsigned priority) {
   fprintf(stderr, "SimpleFactoryRegister: priority=%u, dup name: %s\n", priority, name.ToString().c_str());
}

} // namespaces
