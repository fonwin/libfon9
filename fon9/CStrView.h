// \file fon9/CStrView.h
// \author fonwinz@gmail.com
#ifndef __fon9_CStrView_h__
#define __fon9_CStrView_h__

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

/// 使用時, 除非有特別的說明,
/// Begin_ 可以視為一般的 c 字串, *(End_) 為 '\0';
struct fon9_CStrView_t {
   const char* Begin_;
   const char* End_;
};
typedef struct fon9_CStrView_t    fon9_CStrView;

#ifdef __cplusplus
}//extern "C"

template <class StrT>
inline auto fon9_CStrViewFrom(fon9_CStrView& dst, const StrT& str)
-> decltype(str.c_str(), void()) {
   dst.Begin_ = str.c_str();
   dst.End_ = dst.Begin_ + str.size();
}

template <class StrT>
inline auto fon9_ToCStrView(const StrT& str) -> decltype(fon9_CStrView{str.c_str(),str.c_str()+str.size()}) {
   fon9_CStrView retval;
   fon9_CStrViewFrom(retval, str);
   return retval;
}
template <class StrT>
inline auto fon9_ToCStrView(const StrT& str) -> decltype(fon9_CStrView(ToStrView(str).ToCStrView())) {
   return ToStrView(str).ToCStrView();
}
#endif//__cplusplus

#endif//__fon9_CStrView_h__
