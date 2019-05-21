/// \file fon9/ObjSupplier.cpp
/// \author fonwinz@gmail.com
#include "fon9/ObjSupplier.hpp"

namespace fon9 {

ObjArrayMem::~ObjArrayMem() {
   assert(this->RemainCount_ == 0);
}
//--------------------------------------------------------------------------//

// 適用於每個 instance 有自己的 thread_local 的情況;
// - 須配合 ObjPool; 然後每個 instance 向 ObjPool 申請一個 idx;
// - 每個 thread_local 也用 ObjPool 建立一個表, 然後用 idx 取得;
// - 但如果 ObjectT 的建構參數, 與 idx 的「擁有者」有關時.
//    - 當該 idx 的「擁有者」死亡時, 要如何釋放每個 thread_local 裡面的 pool?
// \code
// \endcode
// template <class ObjectT, uint32_t kObjectCount, uint32_t kExtPoolCount, unsigned kTlsId>
// class ObjSupplierEI;


ObjSupplierBase::~ObjSupplierBase() {
   this->FillTimer_.StopAndWait();
}
void ObjSupplierBase::Timer::EmitOnTimer(TimeStamp now) {
   (void)now;
   ObjSupplierBase&  rthis = ContainerOf(*this, &ObjSupplierBase::FillTimer_);
   rthis.FillPool();
}
void ObjSupplierBase::FillPool() {
   this->FillTimer_.RunAfter(TimeInterval_Millisecond(100));
   Pool::Locker pool{this->Pool_};
   size_t       count = pool->size();
   if (this->ExtPoolCount_ < count)
      return;
   pool.unlock();

   std::deque<ObjArrayMemSP> exPool{this->ExtPoolCount_ - count + 2};
   for (auto& i : exPool)
      i = this->MakeCarrierTape();

   pool.lock();
   for (auto& i : exPool)
      pool->emplace_back(std::move(i));
   pool.unlock();
   // after unlock: dtor exPool: free exPool internal memory.
}
void* ObjSupplierBase::AllocFormNewPool(ObjArrayMemSP& tlsTape) {
   ObjArrayMem* tape;
   {
      Pool::Locker pool{this->Pool_};
      if (pool->empty())
         return nullptr;
      tape = (tlsTape = std::move(pool->back())).get();
      pool->pop_back();
   }
   return tape->Alloc();
}

} // namespace
