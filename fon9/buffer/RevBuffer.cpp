// \file fon9/buffer/RevBuffer.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/RevBuffer.hpp"

namespace fon9 {

void RevBuffer::ForVtblInLib() {
}
char* RevBufferFixedMem::OnRevBufferAlloc(size_t) {
   Raise<BufferOverflow>("RevBufferFixedMem overflow");
}

} // namespace
