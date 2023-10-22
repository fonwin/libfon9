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
# 建立必要的 Device factory, 放在 DeviceFactoryPark=FpDevice
ss,Enabled=Y,EntryName=TcpServer,Args='Name=TcpServer|AddTo=FpDevice' /MaPlugins/DevTcps
ss,Enabled=Y,EntryName=TcpClient,Args='Name=TcpClient|AddTo=FpDevice' /MaPlugins/DevTcpc
ss,Enabled=Y,EntryName=Dgram,Args='Name=Dgram|AddTo=FpDevice'         /MaPlugins/DevDgram

# 建立 io 管理員, DeviceFactoryPark=FpDevice; SessionFactoryPark=FpSession; 設定儲存在 fon9cfg/MaIo.f9gv
ss,Enabled=Y,EntryName=NamedIoManager,Args='Name=MaIo|DevFp=FpDevice|SesFp=FpSession|Cfg=MaIo.f9gv|SvcCfg="ThreadCount=2|Capacity=100"' /MaPlugins/MaIo

# 建立 f9card 支援的通訊設備, 並綁定提供服務的 CPU core: Cpu=CardId.EthId:BindCpuCoreId
ss,Enabled=Y,EntryName=F9Card,Args='Tcpc=F9Tcpc|AddTo=FpDevice|Tcps=F9Tcps|AddTo=FpDevice|Udp=F9Udp|AddTo=FpDevice|Cpu=0.0:4|Cpu=0.1:5' /MaPlugins/DevF9Card
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
底下範例中的 IP address, 應配合實際的測試環境設定!
```
# 參數 Args 設定請參考 f9tws/f9twsTrade_UT.cpp
# 底下的 Io 參數, 請參考 fon9/io/IoServiceArgs.hpp
ss,Enabled=Y,EntryName=f9twsTrade,Args='IoTse={ThreadCount=1|Wait=Block|Cpus=6}|IoOtc={ThreadCount=1|Wait=Block|Cpus=7}' /MaPlugins/f9twsTrade

# ----- 設定 台灣證交所 FIX 連線 -----
ss,Enabled=Y,Session=Fix44,SessionArgs='BrkId=123T|SocketId=03|Pass=9999|Fc=160',Device=TcpClient,DeviceArgs='192.168.1.33:10005|Bind=49003|ReuseAddr=Y'   /f9twsTrade/IoTse/^edit:Config/L123T03
# 使用 f9card 的 tcp client, 若要測試 f9card, 則必須關閉上一行一般網卡的連線(Enabled=N), 避免干擾測試結果.
ss,Enabled=Y,Session=Fix44,SessionArgs='BrkId=123T|SocketId=09|Pass=9999|Fc=160',Device=F9Tcpc,DeviceArgs='192.168.5.33:10005|Bind=0.0.0.0:49009|SNDBUF=8' /f9twsTrade/IoTse/^edit:Config/L123T09

# ----- 設定 台灣證交所 行情接收 -----
# 設定行情接收(Multicast), DeviceArgs 請參考 fon9/io/DgramBase.cpp
# 如果沒收到,請先確認是否被防火牆擋掉了!
ss,Enabled=Y,Session=TseMd,Device=Dgram,DeviceArgs='Group=224.0.100.100|Bind=10000|Interface=192.168.1.34' /f9twsTrade/IoTse/^edit:Config/TseMd1
# 使用 f9card 的 udp: Interface=CardId.EthId.0.0
ss,Enabled=Y,Session=TseMd,Device=F9Udp,DeviceArgs='Group=224.0.100.100|Bind=10000|Interface=0.0.0.0'      /f9twsTrade/IoTse/^edit:Config/TseMd9

# 查看設定及套用時的 SubmitId
gv /f9twsTrade/IoTse/^apply:Config

# 套用設定, 假設上一行指令的結果提示 SubmitId=1
/f9twsTrade/IoTse/^apply:Config submit 1
# ----- IoTse 套用後生效 -----
# ----------------------------

# 查看連線狀態
gv /f9twsTrade/IoTse
# 查看商品最後成交價、量...
ps /f9twsTrade/Symbs/2317^LastDeal
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

# 條件新單指令範例
/f9twsTrade/Symbs wait 2317  new  57=0|50=123T  37=C0000|11=C1@1        |55=1101  |1=0000010|38=10|40=2|44=35.4|59=0|10001=0|10004=N|10000=1|10002=0|54=1
```

### 使用 f9card 的測試數據
* 原始程式 [f9twsTrade_UT.cpp](https://github.com/fonwin/libfon9/blob/master/f9tws/f9twsTrade_UT.cpp)
* 底下時間單位 = ns;
* 有「*REF*」標記的為: 行情封包.
* Time 為 [f9pcap 抓包設備](https://docs.google.com/document/d/1jvFZqgNhkOePk5BdvDj9qwYL0AH6YqeqJRwUi9MNaGQ/edit), 使用「DAC 線路」擷取到的封包時間.
  * 封包在「網路線上」完整傳送「結束」的時間點.
* 第1次觸發(5筆下單):  首筆下單封包距離行情, 延遲 3994 ns, 後續4筆, 距離前一筆下單延遲約 8xx..9xx ns;
* 第2次觸發(10筆下單): 首筆下單封包距離行情, 延遲 3322 ns, 後續9筆, 距離前一筆下單延遲約 8xx..10xx ns;
* 若將「DAC 線路」改成使用「光纖+分光器」抓包, 則「首筆下單封包距離行情」延遲, 可再減約 300 ns 的抓包設備延遲;
```
No. Time               TimeDiff       Source                SrcPort Destination           DstPort Protocol Length Info
  1 03:26:15.131222000 0.000000000    192.168.5.33          49312   224.0.100.100         10000   UDP      101    49312 → 10000 Len=59
  2 *REF*              *REF*          192.168.5.33          49312   224.0.100.100         10000   UDP      101    49312 → 10000 Len=59
  3 03:26:23.152449750 0.000003994    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
  4 03:26:23.152450652 0.000004896    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
  5 03:26:23.152451548 0.000005792    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
  6 03:26:23.152452432 0.000006676    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
  7 03:26:23.152453315 0.000007559    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
  8 03:26:23.152924432 0.000478676    192.168.5.33          10005   192.168.5.34          49009   TCP      60     10005 → 49009 [ACK] Seq=1 Ack=1111 Win=8207 Len=0
  9 03:26:23.156834928 0.004389172    192.168.5.33          10005   192.168.5.34          49009   FIX      1499   ExecutionReport, ExecutionReport, ExecutionReport, ExecutionReport, ExecutionReport
 10 03:26:23.205133833 0.052688077    192.168.5.34          49009   192.168.5.33          10005   TCP      60     49009 → 10005 [ACK] Seq=1111 Ack=1446 Win=32768 Len=0
 11 03:26:27.952047651 4.799601895    Solarfla_17:48:d1             00:f9:02:03:04:05             ARP      60     Who has 192.168.5.34? Tell 192.168.5.33
 12 03:26:27.952608822 4.800163066    00:f9:02:03:04:05             Solarfla_17:48:d1             ARP      60     192.168.5.34 is at 00:f9:02:03:04:05
 13 *REF*              *REF*          192.168.5.33          49312   224.0.100.100         10000   UDP      101    49312 → 10000 Len=59
 14 03:27:18.659088662 0.000003322    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 15 03:27:18.659089552 0.000004212    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 16 03:27:18.659090460 0.000005120    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 17 03:27:18.659091376 0.000006036    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 18 03:27:18.659092265 0.000006925    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 19 03:27:18.659093155 0.000007815    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 20 03:27:18.659094038 0.000008698    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 21 03:27:18.659094940 0.000009600    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 22 03:27:18.659095984 0.000010644    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 23 03:27:18.659096892 0.000011552    192.168.5.34          49009   192.168.5.33          10005   FIX      276    NewOrderSingle
 24 03:27:18.659514960 0.000429620    192.168.5.33          10005   192.168.5.34          49009   TCP      60     10005 → 49009 [ACK] Seq=1446 Ack=3331 Win=8212 Len=0
 25 03:27:18.664067574 0.004982234    192.168.5.33          10005   192.168.5.34          49009   FIX      1514   ExecutionReport, ExecutionReport, ExecutionReport, ExecutionReport, ExecutionReport
 26 03:27:18.664068790 0.004983450    192.168.5.33          10005   192.168.5.34          49009   FIX      1484   ExecutionReport, ExecutionReport, ExecutionReport, ExecutionReport, ExecutionReport
 27 03:27:18.664119600 0.005034260    192.168.5.34          49009   192.168.5.33          10005   TCP      60     49009 → 10005 [ACK] Seq=3331 Ack=4336 Win=32768 Len=0
```
* 測試環境:
  * 主機版: ASUS ROG MAXIMUS Z790 HERO (關閉 Hyper-Threading)
  * CPU: Intel i9-13900KS
  * RAM: 威剛32GB(雙通16GB*2) D5 7200/CL34 LancerRGB(AX5U7200C3416G-DCLARBK)
  * 行情&下單連線: 綁在 CPU Core 4
  * 測試程式: f9tws/f9twsTrade_UT.cpp
```
$ cat /proc/cmdline
BOOT_IMAGE=/boot/vmlinuz-5.15.0-1045-realtime root=UUID=65eec721-7eff-4015-a4cb-e2490192e3c0 ro skew_tick=1 nohz=on nohz_full=1-7 rcu_nocb_poll rcu_nocbs=1-7 kthread_cpus=8-23 irqaffinity=8-23 isolcpus=managed_irq,domain,1-7 intel_pstate=passive nosoftlockup tsc=nowatchdog processor.ignore_ppc=1

$ lsb_release -a
No LSB modules are available.
Distributor ID:	Ubuntu
Description:	Ubuntu 22.04.2 LTS
Release:	22.04
Codename:	jammy

$ lscpu --extended --all 
CPU NODE SOCKET CORE L1d:L1i:L2:L3 ONLINE    MAXMHZ   MINMHZ      MHZ
  0    0      0    0 0:0:0:0          yes 5500.0000 800.0000 5511.870
  1    0      0    1 4:4:1:0          yes 5500.0000 800.0000 5511.870
  2    0      0    2 8:8:2:0          yes 5500.0000 800.0000 5511.870
  3    0      0    3 12:12:3:0        yes 5500.0000 800.0000 5511.870
  4    0      0    4 16:16:4:0        yes 5800.0000 800.0000 5826.834
  5    0      0    5 20:20:5:0        yes 5800.0000 800.0000 5826.834
  6    0      0    6 24:24:6:0        yes 5500.0000 800.0000 5511.870
  7    0      0    7 28:28:7:0        yes 5500.0000 800.0000 5511.870
  8    0      0    8 32:32:8:0        yes 4300.0000 800.0000 4300.000
  9    0      0    9 33:33:8:0        yes 4300.0000 800.0000 4300.000
  ... 略 ...

$sudo lshw -short -C memory
H/W path             Device     Class          Description
==========================================================
/0/0                            memory         64KiB BIOS
/0/c                            memory         32GiB System Memory
/0/c/0                          memory         [empty]
/0/c/1                          memory         16GiB DIMM Synchronous 7200 MHz (0.1 ns)
/0/c/2                          memory         [empty]
/0/c/3                          memory         16GiB DIMM Synchronous 7200 MHz (0.1 ns)
/0/1b                           memory         384KiB L1 cache
/0/1c                           memory         256KiB L1 cache
/0/1d                           memory         16MiB L2 cache
/0/1e                           memory         36MiB L3 cache
/0/1f                           memory         512KiB L1 cache
/0/20                           memory         1MiB L1 cache
/0/21                           memory         16MiB L2 cache
/0/22                           memory         36MiB L3 cache
/0/100/14.2                     memory         RAM memory
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
