fon9 rc protocol = fon9 Remote Call protocol
============================================

## 一、為何要自創 protocol?
### 為何不用 AMQP 1.0?
- 雖然很想使用，因為以後開發 MOM 時，還是要面對 AMQP
- 在低延遲應用時，很難忽視額外資料的負擔(傳遞時的資料量、編解碼...)
  - AMQP 額外資料太多(header, properties...)
- AMQP 沒有 checksum
### 為何不用 boost.serialization/protobuf/FlatBuffers/MessagePack/yas...?
- 減少外部依賴.
- 沒有支援 Decimal 型別.
- TODO: 比較: 封包的大小、打包解包的速度
### 為何不用現成的 RPC protocol (ONC RPC...)
- RPC 較適合由單一 AP 提供單一服務, 不適合在單一 Server 內提供整套服務.
### 為何要自創 f9rc protocol?
- 必須能「方便 & 正確」傳遞價格(Decimal)
- 必須提供 checksum 確保資料的正確.
  - 但在確定非必要時, 可移除 checksum: 可進一步降低系統負擔 & 降低延遲.

-------------------------

## 二、協定說明
### 名詞說明
- f9rc: fon9 Remote Call protocol
- Bitv: Binary type value encoding protocol
  - f9rc 打包資料的編碼協定.
  - 參考 [`fon9/Bitv.h`](../Bitv.h)
- aBitv: 一個 fon9_BitvT + 後續跟隨的資料(value).
- aComposite: Bitv 資料組合機制.
  - 組合後的資料量, 最多不可超過 kBitvMaxByteArraySize = UINT32_MAX - 5
   
### 連線步驟
#### 1. 建立實體連線
- 使用 TCP 或 TCP+TLS 建立可靠連線.
#### 2. 協定選擇
- 協定選擇字串 = "`協定版本:額外參數\n`"
  - 若沒有「額外參數」則省略「`:`」, 若有多個參數則使用「`,`」分隔.
  - 目前的協定版本為 "`f9rc.0`"
  - 目前可用的額外參數為: (大小寫必須正確)
    - "`Checksum=N`" (預設為Y) 表示送出的封包不使用 Checksum, 可能的原因:
      - 使用 localhost
      - 使用 TLS
      - 重要訊息有簽章, 且 Server 端有啟用驗章.
      - 使用者決定自負「資料可能有錯」的風險. 曾經遇過的實例:
        - 某個網路設備未偵測出 RAM(某bit) 故障, 當封包通過該設備存入故障的 RAM.
        - 如果轉送該封包需要重算 TCP Checksum, 則會用到錯誤的資料, 轉送之後就無法發現資料有誤了!
        - 雖然很快發現問題, 並更換了設備, 但如果是下單封包, 則有可能造成很大的爭議!
- 連線成功後, Client 必須先送出「協定選擇字串」, 做為選擇 f9rc 版本之用.
- 也就是 Client 在 LinkReady 時, 應立即送出: "`f9rc.0\n`" 或 "`f9rc.0:Checksum=N\n`"
  - 可在「協定選擇字串」之後, 緊接第一個要送的封包.
- Server 端收到 Client 送來的「協定選擇字串」之後, 會回覆 Server 端的「協定選擇字串」, 表明 Server 端協定的版本.
  - Server 收到 "`f9rc.0:Checksum=N`", 有可能回覆 "`f9rc.0`", 表示 Server 端仍會送出有 Checksum 的封包.
  - 如果「協定選擇字串」不正確, 則 Server 回覆: "`f9rc.0:err=錯誤訊息\n`"; 然後關閉連線.
#### 3. 登入
協定選擇完畢後, 需進行 Connection & SASL, 成功後才能進入服務模式.
- Connection
- SASL
#### 4. 斷線時機
發生底下情況, 需主動關閉連線:
- 連線成功後 n 秒內需完成 [Connection](#function-code--0x00-connection) & [SASL](#function-code--0x01-sasl), 否則關閉連線
- 必須依序完成 Connection & SASL
  - 若在 SASL 完成前收到其他封包, 或登入成功後收到 Connection or SASL, 則關閉連線.
- 登入成功後:
  - Client 每隔 m 秒若無送出資料: 傳送一個 [Heartbeat](#function-code--0x03-heartbeat).
  - Server 收到 Heartbeat 後, 會回一個 Heartbeat.
  - Server 在 m + a 秒之內, 沒收到任何資料: 直接關閉連線.
  - Client 在 m + a*2 秒之內, 沒收到任何資料: 直接關閉連線.
- Checksum error: 關閉連線.
- Bitv 不正確: 關閉連線.
- 關閉連線前, 可選擇透過「[Logout 封包](#function-code--0x02-logout)」送出關閉連線的原因.

### "`f9rc.0`" 的封包格式
- [2 bytes] Checksum: 若協定選擇時, 有額外參數 "`Checksum=N`" 則無此欄位.
  - 包含 Function code & param.
  - 使用 BigEndian
   ```c++
   uint16_t cksum = 0;
   for (byte ch : packet) {
      cksum = ((cksum & 1) << 15) | (cksum >> 1); // cksum = ror(cksum);
      cksum += ch;
   }
   return cksum;
   ```
- [1 byte]     Function code: 0x00..0xff
- [1..n bytes] Function param: 使用一個 aComposite 儲存參數, 由各個 Function 解釋內容.
  - 如果沒有任何參數, 應使用 fon9_BitvT_Null

### Function param 的設計原則
- 事先約定: 彈性小, 事後調整工程較大; 例: [Heartbeat](#function-code--0x03-heartbeat).
  - 可在最前面加入一個版本號: 使用 CompositeVer<FunctionParam> 機制
  - 若無法調整, 則必須新增 Function code.
- 先查詢所需參數&型別: 較複雜, 但彈性大, 可因應未來變化; 例: [Order request](#function-code--0x08-f9oms-api-order-request)

-------------------------

## 三、功能呼叫 Function code
### 管理功能 0x00..0x07
#### Function code = 0x00: Connection
- C -> S: Connection REQ: "ClientApVer" + "Description"
  - "FomsClient.0.0.1"
  - "fon9 OMS API Client."
- C <- S: Connection ACK: "ServerApVer" + "Description" + "SASL mech list: 使用 ' '(space) 分隔"
  - "FomsServer.0.0.1"
  - "fon9 OMS Server."
  - "PLAIN SCRAM-SHA-1"

#### Function code = 0x01: SASL
- fon9_Auth_R: [`fon9/auth/AuthBase.h`](../auth/AuthBase.h)
- C -> S: SASL start: "SASL mech name" + "client-first-message(如果沒有,則填入空白字串)".
  - e.g. 使用者id="user", 密碼="pencil"
    - "SCRAM-SHA-1"
    - "n,,n=user,r=fyko+d2lbbFgONRv9qkxdawL"
- C <> S: SASL negotiate.
  - 重複底下步驟, 直到 RCode != 1(fon9_Auth_NeedsMore) 則進入 SASL outcome
  - C <- S: RCode(fon9_Auth_R) + ChallengeMessage
    - 1(fon9_Auth_NeedsMore)
    - "r=fyko+d2lbbFgONRv9qkxdawL3rfcNHYJY1ZVvWVs7j,s=QSXCR+Q6sek8bf92, i=4096"
  - C -> S: ResponseMessage
    - "c=biws,r=fyko+d2lbbFgONRv9qkxdawL3rfcNHYJY1ZVvWVs7j,p=v0X8v3Bz2T0CJGbJQyF0X+HI4Ts="
- C <- S: SASL outcome.
  - 失敗: <0(fon9_Auth_R) + "e=error message"
  - 成功: 0(fon9_Auth_Success) + "server-final-message" + "ext info"
  - e.g.
    - 0(fon9_Auth_Success)
    - "v=rmF9pqV8S7suAoZWja4dJRkFsKQ="
    - "上次登入: 2017/09/08 來自 127.0.0.1, 密碼還有3天到期"

#### Function code = 0x02: Logout
- C <> S: 強制登出(斷線)
  - data = 強制登出(斷線)原因.
  - e.g. "Unknown function code."
  - e.g. "Packet frame error."
  - e.g. "Checksum error."

#### Function code = 0x03: Heartbeat
- C -> S: Heartbeat REQ
  - data = 符合 aComposite 規範的任意資料.
- C <- S: Heartbeat ACK
  - Decimal(TimeStamp) AckTime.
  - 原本的 Client data(Function param), 不含 Send() 加上的 ByteArraySizeToBitvT().

#### Function code = 0x07: Seed/Tree
* Query/Subscribe
* Modify/Remove

### f9oms API: 0x08
#### Function code = 0x08: f9oms api
細節請參閱 f9oms api 的說明.
* Query config.
* TDay changed.
* Report recover/subscribe
* Request: 下單要求(新刪改查)
