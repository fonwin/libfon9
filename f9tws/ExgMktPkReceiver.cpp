// \file f9tws/ExgMktPkReceiver.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMktPkReceiver.hpp"

namespace f9tws {

ExgMktPkReceiver::~ExgMktPkReceiver() {
}
unsigned ExgMktPkReceiver::GetPkSize(const void* pkptr) {
   return reinterpret_cast<const ExgMktHeader*>(pkptr)->GetLength();
}

} // namespaces
