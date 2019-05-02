fon9 Order management system for tw
===================================

libf9omstw: 台灣環境的委託管理系統.
* 採用 static library:
  * 執行時的速度可加快一點點。
  * 僅需提供單一執行檔(減少額外的 dll), 可讓執行環境更單純。

## 風控資料表
底下是「f9omstw:台灣環境OMS」所需的資料表, 不同的OMS實作、不同的風控需求, 所需的表格、及表格元素所需的欄位不盡相同。
* Symb
  * 1張的股數, 警示, 平盤下可券賣...
  * SymbRef 今日參考價
* Brk
  * 券商代號: AA-BB 或 AAA-B; 共4碼文數字, BB 或 B 為分公司代號。
  * 期貨商代號: X999BBB; 後3碼為數字序號, 代表分公司碼。
  * 表格使用 std::vector<OmsBrkSP>
  * Brk-Ivac-Subac
* Ivac
  * IvacNo: 999999C 依序編號(6碼序號, C為1數字檢查碼), 但可能會用前置碼表示特殊身份, 例如: 9700000
  * enum IvacNC: 999999 移除檢查碼後的序號。
  * 帳號有分段連續的特性, 因此表格使用 OmsIvacMap = fon9::LevelArray<IvacNC, OmsIvacSP, 3>;
  * 帳號商品表: 庫存、委託累計、成交累計; 用於風控檢查。
* Subac
  * SubacId = 字串。
  * 表格使用 OmsSubacMap = fon9::SortedVectorSet<OmsSubacSP, OmsSubacSP_Comper>;
* OrdNo
  * Brk-Market-Session-OrdNo(fon9::Trie)
* Request/Order
  * RequestId = 依序編號, 所以使用 std::deque

## Thread safety
因為 OMS 牽涉到的資料表很多, 如果每個表有自己的 mutex, 是不切實際的作法。
目前可以考慮的方式有:
* 共用一個 mutex
* 共用一個 fon9::AQueue
* 所有工作集中到單一 thread

## 每日清檔作業
* 方法:
  * 關閉後重啟?
  * 設計 DailyClear 程序?
    * 清除 OrdNoMap
    * 帳號資料:
      * 移除後重新匯入? 如果有註冊「帳號回報」, 是否可以不要重新註冊?
      * 匯入時處理 IsDailyClear?
      * 先呼叫全部帳號的 OnDailyClear(); ?

## Events
* Report Recover
* Order Report
* Market event
  * Preopen/Open/Close
  * Line broken
