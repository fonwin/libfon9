// \file fon9/fmkt/SymbTwa.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTwa_hpp__
#define __fon9_fmkt_SymbTwa_hpp__
#include "fon9/fmkt/SymbTwaBase.hpp"
#include "fon9/fmkt/Symb.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 台灣 「證券」+「期交所」預設的商品基本類別.
/// - Symb + SymbTwaBase
class fon9_API SymbTwa : public Symb, public SymbTwaBase {
   fon9_NON_COPY_NON_MOVE(SymbTwa);
   using base = Symb;

public:
   using base::base;
   ~SymbTwa();
   
   static seed::Fields MakeFields();
};
using SymbTwaSP = intrusive_ptr<SymbTwa>;

} } // namespaces
#endif//__fon9_fmkt_SymbTwa_hpp__
