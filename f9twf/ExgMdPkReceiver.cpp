// \file f9twf/ExgMdPkReceiver.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdPkReceiver.hpp"

namespace f9twf {

ExgMiPkReceiver::~ExgMiPkReceiver() {
}
unsigned ExgMiPkReceiver::GetPkSize(const void* pkptr) {
   return GetPkSize_ExgMi(*reinterpret_cast<const ExgMiHead*>(pkptr));
}

ExgMcPkReceiver::~ExgMcPkReceiver() {
}
unsigned ExgMcPkReceiver::GetPkSize(const void* pkptr) {
   return GetPkSize_ExgMc(*reinterpret_cast<const ExgMcHead*>(pkptr));
}

} // namespaces
