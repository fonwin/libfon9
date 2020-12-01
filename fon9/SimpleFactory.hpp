// \file fon9/SimpleFactory.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_SimpleFactory_hpp__
#define __fon9_SimpleFactory_hpp__
#include "fon9/SortedVector.hpp"
#include "fon9/CharVector.hpp"

namespace fon9 {

template <class Factory>
Factory SimpleFactoryRegister(StrView name, Factory factory, unsigned priority = 0) {
   using Key = CharVector;
   using FactoryMap = SortedVector<Key, std::pair<Factory,unsigned> >;
   static FactoryMap FactoryMap_;
   if (!factory) {
      auto ifind = FactoryMap_.find(Key::MakeRef(name));
      return ifind == FactoryMap_.end() ? nullptr : ifind->second.first;
   }
   auto& ref = FactoryMap_.kfetch(Key::MakeRef(name)).second;
   if (!ref.first || ref.second < priority) {
      ref.first = factory;
      ref.second = priority;
   }
   else
      fprintf(stderr, "SimpleFactoryRegister: dup name: %s\n", name.ToString().c_str());
   return ref.first;
}

template <class FactoryPark>
struct SimpleFactoryPark {
   static void SimpleRegister() {
   }

   template <class Factory, class... ArgsT>
   static void SimpleRegister(StrView name, Factory factory, unsigned priority, ArgsT&&... args) {
      FactoryPark::Register(name, factory, priority);
      SimpleRegister(std::forward<ArgsT>(args)...);
   }

   template <class Factory, class... ArgsT>
   static void SimpleRegister(StrView name, Factory factory, ArgsT&&... args) {
      FactoryPark::Register(name, factory);
      SimpleRegister(std::forward<ArgsT>(args)...);
   }

   template <class... ArgsT>
   SimpleFactoryPark(ArgsT&&... args) {
      SimpleRegister(std::forward<ArgsT>(args)...);
   }

   SimpleFactoryPark() = delete;
};

} // namespaces
#endif//__fon9_SimpleFactory_hpp__
