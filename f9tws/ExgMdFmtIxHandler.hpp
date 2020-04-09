// \file f9tws/ExgMdFmtIxHandler.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmtIxHandler_hpp__
#define __f9tws_ExgMdFmtIxHandler_hpp__
#include "f9tws/ExgMdSystem.hpp"
#include "f9tws/ExgMdFmtIx.hpp"

namespace f9tws {

class f9tws_API ExgMdFmt21Handler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt21Handler);
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmt21Handler();
};
//--------------------------------------------------------------------------//
/// TWSE 格式10;
/// TPEX 格式12;
class f9tws_API ExgMdFmtIxHandler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmtIxHandler);
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmtIxHandler();
};
using ExgMdTwseFmt10Handler = ExgMdFmtIxHandler;
using ExgMdTpexFmt12Handler = ExgMdFmtIxHandler;
//--------------------------------------------------------------------------//
class f9tws_API ExgMdFmt3Handler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt3Handler);
   using base = ExgMdHandlerAnySeq;
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
   void Initialize();
public:
   const size_t         IdxNoCount_;
   const IdxNo* const   IdxNoList_;

   template <size_t N>
   ExgMdFmt3Handler(ExgMdSystem& mdsys, const IdxNo (&idxNoList)[N])
      : base{mdsys}
      , IdxNoCount_{N}
      , IdxNoList_{&idxNoList[0]} {
      this->Initialize();
   }
   virtual ~ExgMdFmt3Handler();

   static ExgMdHandlerSP MakeTwse(ExgMdSystem& mdsys);
   static ExgMdHandlerSP MakeTpex(ExgMdSystem& mdsys);
};

} // namespaces
#endif//__f9tws_ExgMdFmtIxHandler_hpp__
