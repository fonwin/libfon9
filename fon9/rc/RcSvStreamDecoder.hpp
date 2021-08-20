// \file fon9/rc/RcSvStreamDecoder.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSvStreamDecoder_hpp__
#define __fon9_rc_RcSvStreamDecoder_hpp__
#include "fon9/rc/RcSvcReq.hpp"
#include "fon9/SimpleFactory.hpp"

namespace fon9 { namespace rc {

/// - 保存在每個 svc::SeedRec 裡面.
class fon9_API RcSvStreamDecoderNote {
public:
   virtual ~RcSvStreamDecoderNote();
};
using RcSvStreamDecoderNoteSP = std::unique_ptr<RcSvStreamDecoderNote>;

/// - 保存在每個 svc::TabRec 裡面.
class fon9_API RcSvStreamDecoder {
public:
   virtual ~RcSvStreamDecoder();

   virtual RcSvStreamDecoderNoteSP CreateDecoderNote() = 0;

   /// - ack 由 Stream 提供者自訂內容, 不一定是文字.
   /// - rpt 已初始化: rpt.TreePath_; rpt.Layout_; rpt.SeedKey_; rpt.Tab_; rpt.UserData_;
   ///   rpt.ResultCode_ = f9sv_Result_SubrStreamOK;
   ///   其餘為0;
   /// - 衍生者解碼後, 若有需要應觸發事件通知, 然後才返回.
   virtual void OnSubscribeStreamOK(svc::SubrRec& subr, StrView ack,
                                    f9rc_ClientSession& ses, f9sv_ClientReport& rpt,
                                    bool isNeedsLogResult) = 0;

   /// - rpt 已初始化: rpt.TreePath_; rpt.Layout_; rpt.SeedKey_; rpt.Tab_; rpt.UserData_;
   ///   - rpt.ResultCode_ = f9sv_Result_SubrStream*; 已填入對應值;
   ///   - 其餘為0;
   /// - 收到的 stream data 放在 rx.Gv_, 內容由 Stream 自訂, 不一定是文字.
   /// - 衍生者解碼後應觸發事件通知, 然後才返回.
   virtual void DecodeStreamData(svc::RxSubrData& rx, f9sv_ClientReport& rpt) = 0;
   virtual void DecodeStreamRecover(svc::RxSubrData& rx, f9sv_ClientReport& rpt) = 0;
   virtual void DecodeStreamRecoverEnd(svc::RxSubrData& rx, f9sv_ClientReport& rpt) = 0;
   virtual void DecodeStreamEnd(svc::RxSubrData& rx, f9sv_ClientReport& rpt) = 0;
};
using RcSvStreamDecoderSP = std::unique_ptr<RcSvStreamDecoder>;

/// 針對不同的 Stream 對應針對性的 Decoder, 例如:
/// - fon9/rc/RcMdRtsDecoder.cpp: f9sv_AddStreamDecoder_MdRts();
class fon9_API RcSvStreamDecoderFactory {
protected:
   virtual ~RcSvStreamDecoderFactory();
public:
   virtual RcSvStreamDecoderSP CreateStreamDecoder(svc::TreeRec&) = 0;
};

class fon9_API RcSvStreamDecoderPark : public SimpleFactoryPark<RcSvStreamDecoderPark> {
   using base = SimpleFactoryPark<RcSvStreamDecoderPark>;
public:
   using base::base;
   RcSvStreamDecoderPark() = delete;

   /// - 若 factory == nullptr 則用 name 查找先前註冊的 factory.
   /// - 若 factory != nullptr 且 name 重複, 則註冊失敗, 傳回先前的 factory,
   ///   同時使用 fprintf(stderr, ...); 輸出錯誤訊息.
   /// - 若 factory != nullptr 且 name 沒重複, 則註冊成功, 傳回 factory.
   /// - factory 必須在註冊後依然保持不變. 註冊表僅保留 factory 的指標, 沒有複製其內容.
   static RcSvStreamDecoderFactory* Register(StrView name, RcSvStreamDecoderFactory* factory);
};

} } // namespaces
#endif//__fon9_rc_RcSvStreamDecoder_hpp__