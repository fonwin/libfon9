// \file f9twf/ExgTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTypes_hpp__
#define __f9twf_ExgTypes_hpp__
#include "f9twf/Config.h"
#include "fon9/CharAry.hpp"

namespace f9twf {

using BrkId = fon9::CharAryF<7>;
using OrdNo = fon9::CharAryF<5>;
using FcmId = uint16_t;
using SessionId = uint16_t;

enum class ExgSystemType : uint8_t {
   /// 正常交易時段.選擇權.
   OptRegular = 10,
   /// 正常交易時段.期貨.
   FutRegular = 20,
   /// 盤後交易時段.選擇權.
   OptAfterHour = 11,
   /// 盤後交易時段.期貨.
   FutAfterHour = 21,
};

} // namespaces
#endif//__f9twf_ExgTypes_hpp__
