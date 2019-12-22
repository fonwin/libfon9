// \file fon9/buffer/RevBufferList.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 {

char* RevBufferList::OnRevBufferAlloc(size_t sz) {
   this->SetFrontNodeUsed();
   this->ResetMemPtr(this->Builder_.NewFront(sz));
   return this->MemCurrent_;
}

} // namespace
