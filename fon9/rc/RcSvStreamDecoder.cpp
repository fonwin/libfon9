// \file fon9/rc/RcSvStreamDecoder.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSvStreamDecoder.hpp"

namespace fon9 { namespace rc {

RcSvStreamDecoderNote::~RcSvStreamDecoderNote() {
}
RcSvStreamDecoder::~RcSvStreamDecoder() {
}
RcSvStreamDecoderFactory::~RcSvStreamDecoderFactory() {
}
RcSvStreamDecoderFactory* RcSvStreamDecoderPark::Register(StrView name, RcSvStreamDecoderFactory* factory) {
   return SimpleFactoryRegister(name, factory);
}


} } // namespaces
