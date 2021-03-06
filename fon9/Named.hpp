﻿/// \file fon9/Named.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Named_hpp__
#define __fon9_Named_hpp__
#include "fon9/StrView.hpp"

namespace fon9 {

inline bool IsValidName1stChar(char ch) {
   //return isalpha(static_cast<unsigned char>(ch)) || ch == '_';//第一個字元僅允許 alpha or '_'
   return isalnum(static_cast<unsigned char>(ch)) || ch == '_';//第一個字元僅允許 alpha or number or '_'
}

inline bool IsValidNameChar(char ch) {
   return isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

/// 尋找第一個不可出現在名字中的字元, 則傳回該字元的位置.
/// - Name 只能包含 IsValidNameChar() 所允許的字元.
/// - 第一個字元必須為 IsValidName1stChar() 所允許的字元.
///
/// \retval nullptr   str 全部的字元都可出現在名字之中.
/// \retval str.end() str 為空字串: str.empty();
/// \retval else      不可出現在名字中的字元位置.
fon9_API const char* FindInvalidNameChar(StrView str);

/// 驗證 name 的合法性.
/// \retval true  名稱完全合法.
/// \retval false name有不合法的字元, 或為空字串.
inline bool ValidateName(StrView name) {
   return (FindInvalidNameChar(name) == nullptr);
}

/// \ingroup seed
/// 一個有名字的物件, 利於管理及辨識.
/// 可以在資料交換時(例如:傳遞到ClientUI), 根據[名稱], 從翻譯檔取出該語系的物件說明(Title, Description).
///   - Name_        物件名稱, 例如: "UserID"
///   - Title_       標題文字, 例如: "登入代號"
///   - Description_ 詳細描述, 例如: "只允許英數字,最短6碼,最長12碼"
class fon9_API Named {
   Named& operator=(const Named& named) = delete;
   Named& operator=(Named&& named) = delete;
public:
   /// 物件名稱, 只能在建構時指定.
   const std::string Name_;

   Named() = default;
   Named(const Named& named) = default;
   Named(Named&& named) = default;

   explicit Named(std::string name, std::string title = std::string{}, std::string description = std::string{})
      : Name_(std::move(name))
      , Title_(std::move(title))
      , Description_(std::move(description)) {
      assert(this->Name_.empty() || ValidateName(&this->Name_));
   }

   /// 取得物件標題用的字串: Title. 可能為 empty.
   const std::string& GetTitle() const {
      return this->Title_;
   }
   void SetTitle(std::string value) {
      this->Title_ = std::move(value);
   }
   /// 當 GetTitle().empty() 則傳回 Name_;
   const std::string& GetTitleOrName() const {
      return this->Title_.empty() ? this->Name_ : this->Title_;
   }

   /// 取得物件詳細描述文字. 可能為 empty.
   const std::string& GetDescription() const {
      return this->Description_;
   }
   /// 設定物件詳細描述文字.
   void SetDescription(std::string value) {
      this->Description_ = std::move(value);
   }

protected:
   /// 物件的標題文字.
   std::string Title_;
   /// 物件的詳細描述.
   std::string Description_;
};

/// 從 cfg 取出一個 Named 物件.
/// - 格式: `Name|Title|Description\n`
///   - chSpl = '|'
///   - chTail = '\n'
/// - 先取從 cfg 找到 chTail 或到 cfg.end(), 取出的子字串再用 chSpl 分隔欄位.
/// - chTail == -1 表示不考慮結數字元, 直接使用 cfg.end();
/// - Name, Title, Descrption 支援引號.
///   - Description 如果有加引號, 則引號後, 到 chTail 之間的文字, 全部 ignore.
/// - 如果 retval.Name_.empty() 則: cfg.begin() = 錯誤位置.
///   - retval.Name_ 必須符合 Named::ValidateName() 的檢查.
/// - 返回時, cfg.begin() 移動到 (chTail位置 + 1) 或 cfg.end().
/// - 若有提供 exNameParam, 則在 name 部分, 遇到的第1個「非命名字元」之後, 會放到 *exNameParam 裡面.
///   例如: cfg "Name : ExParam, Title, Desc"; 則 *exNameParam = " : ExParam";
fon9_API Named DeserializeNamed(StrView& cfg, char chSpl, int chTail, StrView* exNameParam = nullptr);

/// 輸出名稱設定, 若 chSpl = '|'; chTail = '\n'; 可能的輸出:
/// - `Name\n`
/// - `Name|'Title'\n`
/// - `Name||'Description'\n`
/// - `Name|'Title'|'Description'\n`
/// - Title, Description 若有特殊字元, 則前後會加上單引號.
/// - 若 chTail == -1; 則表示不加尾端字元.
fon9_API void SerializeNamed(std::string& dst, const Named& named, char chSpl, int chTail);

class RevBuffer;
/// 輸出名稱設定.
/// - 若 named.Description 有特殊字元, 則使用 StrView_ToEscapeStr() 之後, 加上引號.
/// - 若 named.Title 有特殊字元或 chSpl, 則使用 StrView_ToEscapeStr() 之後, 加上引號.
fon9_API void RevPrintNamed(RevBuffer& rbuf, const Named& named, char chSpl);
/// 輸出 named 的 Title 及 Description; 請參考 RevPrintNamed(); 的說明.
fon9_API void RevPrintNamedDesc(RevBuffer& rbuf, const Named& named, char chSpl);

} // namespaces
#endif//__fon9_Named_hpp__
