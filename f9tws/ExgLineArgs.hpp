// \file f9tws/ExgLineArgs.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgLineArgs_hpp__
#define __f9tws_ExgLineArgs_hpp__
#include "f9tws/ExgTypes.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/CharAry.hpp"

namespace f9tws {

fon9_WARN_DISABLE_PADDING;
struct f9tws_API ExgLineArgs {
   f9fmkt_TradingMarket Market_;
   BrkId                BrkId_;
   fon9::CharAryF<2>    SocketId_;
   uint16_t             PassCode_;

   void Clear();

   /// 不改變 args.Market_ 您必須自行處理.
   /// - tag = "BrkId=|SocketId=|Pass=";
   /// - SocketId 與 PvcId 相同.
   /// - true = 成功;
   /// - false.
   ///   - 若返回時 value.IsNull(): 不認識的 tag;
   ///   - 否則為 value 有錯, value.begin() 為錯誤的位置.
   bool Parse(fon9::StrView tag, fon9::StrView& value);

   /// 檢查欄位是否正確.
   std::string Verify() const;

   /// if (value.IsNull()) return tag.ToString("Unknown tag: ");
   /// return tag.ToString() + value.ToString(" unknown value: ");
   static std::string MakeErrMsg(fon9::StrView tag, fon9::StrView value);
};
fon9_WARN_POP;

template <class ExgLineArgsT>
std::string ExgLineArgsParser(ExgLineArgsT& args, fon9::StrView cfg) {
   args.Clear();
   fon9::StrView tag, value;
   while (fon9::StrFetchTagValue(cfg, tag, value)) {
      if (!args.Parse(tag, value))
         return args.MakeErrMsg(tag, value);
   }
   return args.Verify();
}

} // namespaces
#endif//__f9tws_ExgLineArgs_hpp__
