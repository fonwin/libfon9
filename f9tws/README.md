fon9 台灣證券市場
=================

## 基本說明
* 提供 TSE/OTC 行情格式解析
* 提供 TSE/OTC FIX 下單模組

## 基礎元件

## 下單範例程式 f9twsTrade_UT
### 初次啟動前準備事項
* 建立 fon9cfg/fon9common.cfg
```
$LogFileFmt=./logs/{0:f+'L'}/fon9sys-{1:04}.log
$MemLock = Y
```

### 使用「管理模式」啟動程式
$ f9twsTrade_UT --admin
* 可使用 [Ctrl-C] 安全的結束程式, 或輸入 quit 然後 <Enter> 指令結束.
* 初次啟動執行底下指令, 設定通訊基礎功能, 底下的設定都是立即生效:
```
# 建立必要的 Device(TcpClient) factory, 放在 DeviceFactoryPark=FpDevice
ss,Enabled=Y,EntryName=TcpServer,Args='Name=TcpServer|AddTo=FpDevice' /MaPlugins/DevTcps
ss,Enabled=Y,EntryName=TcpClient,Args='Name=TcpClient|AddTo=FpDevice' /MaPlugins/DevTcpc

# 建立 io 管理員, DeviceFactoryPark=FpDevice; SessionFactoryPark=FpSession; 設定儲存在 fon9cfg/MaIo.f9gv
ss,Enabled=Y,EntryName=NamedIoManager,Args='Name=MaIo|DevFp=FpDevice|SesFp=FpSession|Cfg=MaIo.f9gv|SvcCfg="ThreadCount=2|Capacity=100"' /MaPlugins/MaIo
```

### 設定管理員帳號(fonwin)、密碼、權限:
```
ss,RoleId=admin,Flags=0 /MaAuth/UserSha256/fonwin
/MaAuth/UserSha256/fonwin repw Password(You can use a sentence.)
ss,HomePath=/ /MaAuth/PoAcl/admin
ss,Rights=xff /MaAuth/PoAcl/admin/'/'
ss,Rights=xff /MaAuth/PoAcl/admin/'/..'
```

### 啟動證交所下單連線
```
# 參數 Args 設定請參考 f9twsTrade/f9twsTrade_UT.cpp
# 底下的 Io 參數, 請參考 fon9/io/IoServiceArgs.hpp
ss,Enabled=Y,EntryName=f9twsTrade,Args='IoTse={ThreadCount=1|Wait=Block|Cpus=6}|IoOtc=={ThreadCount=1|Wait=Block|Cpus=7}' /MaPlugins/f9twsTrade

# ----- 設定台灣證交所 FIX 連線 -----
ss,Enabled=Y,Session=Fix44,SessionArgs='BrkId=123T|SocketId=03|Pass=9999|Fc=160',Device=TcpClient,DeviceArgs='192.168.1.33:10005|Bind=49003|ReuseAddr=Y' /f9twsTrade/IoTse/^edit:Config/L123T03

# 查看設定及套用時的 SubmitId
gv /f9twsTrade/IoTse/^apply:Config

# 套用設定, 假設上一行指令的結果提示 SubmitId=1
/f9twsTrade/IoTse/^apply:Config submit 1
# ----- IoTse 套用後生效 -----
# ----------------------------

# 查看連線狀態
gv /f9twsTrade/IoTse
```

### 送出下單指令
* 底下指令分成3部分: `cmd FixHeader FixBody` 中間用空白分隔,
  FixHeader 不可有空白, FixHeader 之後, 全部都是 FixBody。
  * new(新單)  FixHeader  FixBody
  * chg(改單)  FixHeader  FixBody
  * del(刪單)  FixHeader  FixBody
* 下單FIX欄位,細節請參考交易所文件說明
  * FixHeader:
    * 57=盤別(0:盤中整股,2:盤後零股,7:盤後定價,C:盤中零股)
    * 50=下單帳號所屬券商(4碼)
  * FixBody 底下欄位可以任意順序:
    * 1=下單帳號(填滿7碼,前方補0)
    * 37=委託書號(5碼)
    * 11=ClOrdID(12碼填滿,補空白)
    * 41=OrigClOrdID(12碼填滿,補空白)
    * 55=商品(股票)代號(6碼,尾端補空白)
    * 38=OrderQty
    * 40=價格別(1:市價,2:限價)
    * 44=價格
    * 54=Side(1:買進,2賣出)
    * 59=TimeInForce(0:ROD,3:IOC,4:FOK)
    * 60=TransactTime(下單時間:由此範例程式自動填入)
    * 10000=TwseIvacnoFlag(1:Normal,2:ATM,3:DMA,4:Internet,5:Voice,6:API)
    * 10001=TwseOrdType(0:Normal,1:代辦融資,2:代辦融券,3:自辦融資,4:自辦融券,5:SBL5,6:SBL6)
    * 10002=TwseExCode(0:盤中整股or盤後定價, 2:盤中零股or盤後零股)
    * 10004=TwseRejStaleOrd是否需要檢查 TransactTime 的時間是否逾時.
```
# 新單指令範例
/f9twsTrade/IoTse  new  57=0|50=123T  37=B0000|11=1@1         |55=1101  |1=0000010|38=10|40=2|44=35.4|59=0|10001=0|10004=N|10000=1|10002=0|54=1

# 減1張,指令範例
/f9twsTrade/IoTse  chg  57=0|50=123T  37=B0000|41=1@1         |11=3@1         |55=1101  |1=0000010|38=1|40=2|44=0|10001=0|10004=N|10000=1|10002=0|54=1

# 改價指令範例
/f9twsTrade/IoTse  chg  57=0|50=123T  37=B0000|41=1@1         |11=2@1         |55=1101  |1=0000010|38=0|40=2|44=35.5|10001=0|10004=N|10000=1|10002=0|54=1

# 刪單指令範例
/f9twsTrade/IoTse  del  57=0|50=123T  37=B0000|41=1@1         |11=4@1         |55=1101  |1=0000010|10004=N|10000=1|10002=0|54=1
```

### 提供 http 管理介面 (選項)
* 將必要檔案複製到 wwwroot/*
  * 將 `fon9/fon9/web/HttpStatic.cfg` 複製到執行路徑下的 `wwwroot/.`
  * 將 `fon9/fon9/web/ma/*` 複製到執行路徑下的 `wwwroot/ma/.`
* 載入必要的 plugins:
```
# 建立 Http request 靜態分配器, wwwroot/HttpStatic.cfg 可參考 fon9/web/HttpStatic.cfg
ss,Enabled=Y,EntryName=HttpStaticDispatcher,Args='Name=HttpRoot|Cfg=wwwroot/HttpStatic.cfg' /MaPlugins/Http-R

# 建立 Http session factory, 使用剛剛建立的 http 分配器(HttpRoot), 放在 SessionFactoryPark=FpSession
ss,Enabled=Y,EntryName=HttpSession,Args='Name=HttpMa|HttpDispatcher=HttpRoot|AddTo=FpSession' /MaPlugins/HttpSes

# 建立 WsSeedVisitor(透過 web service 管理 fon9::seed), 使用 AuthMgr=MaAuth 檢核帳密權限, 放在剛剛建立的 http 分配器(HttpRoot)
ss,Enabled=Y,EntryName=WsSeedVisitor,Args='Name=WsSeedVisitor|AuthMgr=MaAuth|AddTo=HttpRoot' /MaPlugins/HttpWsMa
```
* 在 io 管理員(MaIo) 設定 HttpSession:
```
# 建立設定, 此時尚未套用(也尚未存檔)
ss,Enabled=Y,Session=HttpMa,Device=TcpServer,DeviceArgs=6080 /MaIo/^edit:Config/MaHttp

# 查看設定及套用時的 SubmitId
gv /MaIo/^apply:Config

# 套用設定, 假設上一行指令的結果提示 SubmitId=123
/MaIo/^apply:Config submit 123
```
