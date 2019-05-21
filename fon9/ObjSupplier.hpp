/// \file fon9/ObjSupplier.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ObjSupplier_hpp__
#define __fon9_ObjSupplier_hpp__
#include "fon9/Timer.hpp"
#include <deque>

namespace fon9 {

/// \ingroup Misc
/// 不會直接使用此 class, 您應該使用 ObjCarrierTape<>.
/// - Clear(); Alloc(); **不是 thread safe**
/// - 一塊包含 ObjectCount_ 個 Objects 物件的記憶體.
/// - 這裡定義的是頭端的管理資料.
/// - 當全部的 Objects 都死亡時才會釋放 this;
class fon9_API ObjArrayMem : protected intrusive_ref_counter<ObjArrayMem> {
   fon9_NON_COPY_NON_MOVE(ObjArrayMem);
protected:
   uint32_t RemainCount_;

public:
   const uint32_t ObjectCount_;
   const uint16_t ArrayOffset_;
   const uint16_t ObjectSize_;

   fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
   template <class ObjT, size_t kCount>
   ObjArrayMem(ObjT (&array)[kCount])
      : RemainCount_{kCount}
      , ObjectCount_{kCount}
      , ArrayOffset_{static_cast<uint16_t>(reinterpret_cast<byte*>(&array[0]) - reinterpret_cast<byte*>(this))}
      , ObjectSize_{static_cast<uint16_t>(reinterpret_cast<byte*>(&array[1]) - reinterpret_cast<byte*>(&array[0]))} {
      assert(reinterpret_cast<byte*>(this) + this->ArrayOffset_ == reinterpret_cast<byte*>(&array[0]));
      assert(this->ObjectSize_ == (reinterpret_cast<byte*>(&array[1]) - reinterpret_cast<byte*>(&array[0])));
   }
   fon9_MSC_WARN_POP;

   virtual ~ObjArrayMem();

   virtual void Clear() = 0;

   void* Alloc() {
      if (fon9_LIKELY(this->RemainCount_ > 0))
         return reinterpret_cast<byte*>(this) + this->ArrayOffset_ + this->ObjectSize_ * (--this->RemainCount_);
      return nullptr;
   }

   size_t RemainCount() const {
      return this->RemainCount_;
   }

   /// ObjArrayMem 的擁有者指標, 類似 std::unique_ptr 的功能.
   /// - 在擁有者死亡時, 會呼叫 ObjArrayMem::Clear 清除剩餘未分配的物件.
   /// - 但要等到已分配的物件也死光時, 才會釋放 ObjArrayMem.
   class ThisSP {
      fon9_NON_COPYABLE(ThisSP);
      ObjArrayMem*   Ptr_;
   public:
      ThisSP(ObjArrayMem* p = nullptr) : Ptr_{p} {
         if (p)
            intrusive_ptr_add_ref(p);
      }
      ~ThisSP() {
         if (this->Ptr_) {
            this->Ptr_->Clear();
            intrusive_ptr_release(this->Ptr_);
         }
      }

      ThisSP(ThisSP&& rhs) : Ptr_{rhs.Ptr_} {
         rhs.Ptr_ = nullptr;
      }
      ThisSP& operator=(ThisSP&& rhs) {
         ThisSP old{std::move(*this)};
         this->Ptr_ = rhs.Ptr_;
         rhs.Ptr_ = nullptr;
         return *this;
      }
      void swap(ThisSP& rhs) {
         ObjArrayMem* rhsPtr = rhs.Ptr_;
         rhs.Ptr_ = this->Ptr_;
         this->Ptr_ = rhsPtr;
      }

      ObjArrayMem* operator->() const {
         assert(this->Ptr_ != nullptr);
         return this->Ptr_;
      }
      ObjArrayMem* get() const {
         return this->Ptr_;
      }
      ObjArrayMem* release() {
         ObjArrayMem* retval = this->Ptr_;
         this->Ptr_ = nullptr;
         return retval;
      }
   };

   class ObjInjection {
      friend class ObjArrayMem;
      ObjArrayMem*   MemOwner_{nullptr};
   public:
      ~ObjInjection() {
         assert(this->MemOwner_ == nullptr);
      }
      template<class Derived>
      static void FreeObject(Derived* pDerived) {
         if (pDerived->MemOwner_)
            pDerived->MemOwner_->FreeObject(pDerived);
         else
            delete pDerived;
      }
   };

protected:
   void SetObjectOwner(ObjInjection* pobj) {
      assert(pobj->MemOwner_ == nullptr);
      pobj->MemOwner_ = this;
      intrusive_ptr_add_ref(this);
   }

   template<class Derived>
   void FreeObject(Derived* pobj) {
      assert(static_cast<ObjInjection*>(pobj)->MemOwner_ == this);
      assert(reinterpret_cast<byte*>(this) + this->ArrayOffset_ <= reinterpret_cast<byte*>(pobj)
             && reinterpret_cast<byte*>(pobj) < reinterpret_cast<byte*>(this) + this->ArrayOffset_ + this->ObjectSize_ * this->ObjectCount_);
      static_cast<ObjInjection*>(pobj)->MemOwner_ = nullptr;
      pobj->~Derived();
      intrusive_ptr_release(this);
   }
};
using ObjArrayMemSP = ObjArrayMem::ThisSP;

/// \ingroup Misc
/// Object Carrier Tape.
/// - Clear(); Alloc(); **不是 thread safe**
/// - 用於想要快速分配「已初始化的物件」
/// - 物件全部釋(Alloc + FreeThis)放後, 才會釋放 this;
/// - 適用於: 物件分配後幾乎不會釋放(或每日釋放全部)的情境.
/// - 例如: OMS 的下單要求、委託書, 這類的應用.
///   因為這些物件一旦分配, 就會一直保留到隔日清檔.
/// - ObjectT 必須提供保護: 從這裡分配出去的物件禁止直接 delete, 必須透過 FreeThis() 釋放;
///   \code
///   class Object {
///   protected:
///      // 禁止直接 delete this; 需透過 FreeThis() 釋放;
///      virtual ~Object();
///   public:
///      // 必須透過 FreeThis() 來刪除, 預設 delete this;
///      // 若有使用 ObjCarrierTape 則將 this 還給 ObjCarrierTape;
///      virtual void FreeThis() { delete this; }
///   };
///   \endcode
template <class ObjectT, uint32_t kCount>
class ObjCarrierTape : public ObjArrayMem {
   fon9_NON_COPY_NON_MOVE(ObjCarrierTape);
   using base = ObjArrayMem;

   fon9_MSC_WARN_DISABLE(4582// 'ObjectArray_' : constructor is not implicitly called
                         4583// 'ObjectArray_' : destructor is not implicitly called
                         4355// 'this' : used in base member initializer list
   );
   // 僅允許在 intrusive_ptr_release() 時刪除.
   ~ObjCarrierTape() {
   }

public:
   /// args = ObjectT 的建構參數.
   template <class... ArgsT>
   ObjCarrierTape(ArgsT&&... args) : base{this->ObjectArray_} {
      memset(&this->ObjectArray_, 0, sizeof(this->ObjectArray_));
      auto* iobj = &this->ObjectArray_[0];
      for (uint32_t L = 0; L < kCount; ++L, ++iobj) {
         auto* pobj = InplaceNew<TapedObject>(&*iobj, std::forward<ArgsT>(args)...);
         this->SetObjectOwner(pobj);
      }
   }
   fon9_MSC_WARN_POP;

   fon9_MSC_WARN_DISABLE(4623); // default constructor was implicitly defined as deleted
   struct TapedObject final : public ObjectT, public ObjInjection {
      fon9_NON_COPY_NON_MOVE(TapedObject);
      using ObjectT::ObjectT;
      TapedObject() = default;
      void FreeThis() { // override
         ObjInjection::FreeObject(this);
      }
   };
   fon9_MSC_WARN_POP;

   class ThisSP : public ObjArrayMemSP {
      fon9_NON_COPYABLE(ThisSP);
      using base = ObjArrayMemSP;
   public:
      ThisSP(ObjCarrierTape* p = nullptr) : base{p} {
      }
      ThisSP(ThisSP&& rhs) = default;
      ThisSP& operator=(ThisSP&& rhs) = default;
      ObjCarrierTape* operator->() const {
         assert(this->get() != nullptr);
         return this->get();
      }
      ObjCarrierTape* get() const {
         return static_cast<ObjCarrierTape*>(this->base::get());
      }
   };

   TapedObject* Alloc() {
      if (fon9_LIKELY(this->RemainCount_ > 0))
         return &this->ObjectArray_[--this->RemainCount_];
      return nullptr;
   }

   void Clear() override {
      while (this->RemainCount_ > 0)
         this->FreeObject(&this->ObjectArray_[--this->RemainCount_]);
   }

private:
   union { // ObjectArray_ 放在 union 裡面: 建構時不要初始化!
      TapedObject ObjectArray_[kCount];
   };
};

/// \ingroup Misc
/// 不會直接使用此 class, 您應該使用 ObjSupplier<>.
class fon9_API ObjSupplierBase : public intrusive_ref_counter<ObjSupplierBase> {
   fon9_NON_COPY_NON_MOVE(ObjSupplierBase);
   const uint32_t ExtPoolCount_;
      
   struct Timer : public DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(Timer);
      Timer() = default;
      void EmitOnTimer(TimeStamp now) override;
   };
   Timer FillTimer_;

   using PoolImpl = std::deque<ObjArrayMemSP>;
   using Pool = fon9::MustLock<PoolImpl>;
   Pool  Pool_;

   virtual ObjArrayMemSP MakeCarrierTape() = 0;
   void* AllocFormNewPool(ObjArrayMemSP& tlsTape);

protected:
   void* Alloc(ObjArrayMemSP& tlsTape) {
      if (auto* tape = tlsTape.get()) {
         if (void* pobj = tape->Alloc())
            return pobj;
      }
      return this->AllocFormNewPool(tlsTape);
   }

public:
   ObjSupplierBase(uint32_t extPoolCount)
      : ExtPoolCount_{extPoolCount} {
   }
   virtual ~ObjSupplierBase();

   void FillPool();
};

/// \ingroup Misc
/// - 每個 thread 一個 thread_local ObjCarrierTape;
/// - 當 thread_local ObjCarrierTape; 用完時, 從 Pool 取得一個 ObjCarrierTape;
///   如果 Pool 裡面的 ObjCarrierTape 也用完(尚未補充), 則直接建立「一個」新的 ObjectT;
/// - 定時(例如:0.1秒), 補充 Pool 裡面的 ObjCarrierTape;
/// - ObjectT 必須提供預設建構: new ObjectT{}.
template <class ObjectT, uint32_t kObjectCount, uint32_t kExtPoolCount = 1>
class ObjSupplier : public ObjSupplierBase {
   fon9_NON_COPY_NON_MOVE(ObjSupplier);
   using base = ObjSupplierBase;

   /// 僅能使用 Make() 建立一個 singleton;
   ObjSupplier() : base{kExtPoolCount} {
      this->FillPool();
   }

   using CarrierTape = ObjCarrierTape<ObjectT, kObjectCount>;
   ObjArrayMemSP MakeCarrierTape() override {
      return new CarrierTape{};
   }
public:
   using ThisSP = intrusive_ptr<ObjSupplier>;
   using TapedObject = typename CarrierTape::TapedObject;

   /// 即使呼叫多次 Make() 傳回的都是同一個.
   static ThisSP Make() {
      static ThisSP  singleton{new ObjSupplier};
      return singleton;
   }

   ObjectT* Alloc() {
      thread_local ObjArrayMemSP TlsTape_;
      if (void* pobj = base::Alloc(TlsTape_))
         return reinterpret_cast<TapedObject*>(pobj);
      return new ObjectT{};
   }
};

} // namespaces
#endif//__fon9_ObjSupplier_hpp__
