// \file fon9/rc/RcMdRtsDecoder.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcMdRtsDecoder_hpp__
#define __fon9_rc_RcMdRtsDecoder_hpp__
#include "fon9/fmkt/MdRtsTypes.hpp"
#include "fon9/rc/RcSvStreamDecoder.hpp"

namespace fon9 { namespace rc {
   
extern fon9_API struct f9sv_SvStreamDecoder f9sv_MdRts;

/// \ingroup rc
/// 提供給 RcSeedVisitorClient 使用, 解碼 fon9::fmkt::MdRtStream;
class fon9_API RcMdRtsDecoder {
   fon9_NON_COPY_NON_MOVE(RcMdRtsDecoder);

   DayTime  InfoTime_{DayTime::Null()};
   /// 回補與即時可能同時傳送, 回補時是否要另建一個 Pod 保存回補的內容?
   DayTime  RecoverInfoTime_{DayTime::Null()};

public:
   RcMdRtsDecoder() = default;

   DayTime InfoTime() const {
      return this->InfoTime_;
   }
};

} } // namespaces
#endif//__fon9_rc_RcMdRtsDecoder_hpp__
