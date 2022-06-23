// \file f9tws/ExgMdPkReceiver.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdPkReceiver.hpp"

namespace f9tws {

ExgMdPkReceiver::~ExgMdPkReceiver() {
}
unsigned ExgMdPkReceiver::GetPkSize(const void* pkptr) {
   return reinterpret_cast<const ExgMdHead*>(pkptr)->GetLength();
}

} // namespaces
