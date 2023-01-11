fon9 web 管理介面/啟用方法
==========================

* 使用底下操作, 可讓 fon9 提供 web 管理介面, 透過瀏覽器開啟:
http://127.0.0.1:6080/ma/fon9ma.html

* 將必要檔案複製到 wwwroot/*
  * 將 `fon9/fon9/web/HttpStatic.cfg` 複製到執行路徑下的 `wwwroot/.`
  * 將 `fon9/fon9/web/ma/*` 複製到執行路徑下的 `wwwroot/ma/.`

* 載入必要的 plugins:
```
# 啟用 TcpServer
ss,Enabled=Y,EntryName=TcpServer,Args='Name=TcpServer|AddTo=FpDevice' /MaPlugins/DevTcps

# 建立 Http request 靜態分配器, wwwroot/HttpStatic.cfg 可參考 fon9/web/HttpStatic.cfg
ss,Enabled=Y,EntryName=HttpStaticDispatcher,Args='Name=HttpRoot|Cfg=wwwroot/HttpStatic.cfg' /MaPlugins/Http-R

# 建立 Http session factory, 使用剛剛建立的 http 分配器(HttpRoot), 放在 SessionFactoryPark=FpSession
ss,Enabled=Y,EntryName=HttpSession,Args='Name=HttpMa|HttpDispatcher=HttpRoot|AddTo=FpSession' /MaPlugins/HttpSes

# 建立 WsSeedVisitor(透過 web service 管理 fon9::seed), 使用 AuthMgr=MaAuth 檢核帳密權限, 放在剛剛建立的 http 分配器(HttpRoot)
ss,Enabled=Y,EntryName=WsSeedVisitor,Args='Name=WsSeedVisitor|AuthMgr=MaAuth|AddTo=HttpRoot' /MaPlugins/HttpWsMa

# 建立 io 管理員, DeviceFactoryPark=FpDevice; SessionFactoryPark=FpSession; 設定儲存在 fon9cfg/MaIo.f9gv
ss,Enabled=Y,EntryName=NamedIoManager,Args='Name=MaIo|DevFp=FpDevice|SesFp=FpSession|Cfg=MaIo.f9gv|SvcCfg="ThreadCount=2|Capacity=100"' /MaPlugins/MaIo
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

* 設定管理員帳號(fonwin)、密碼、權限:
```
ss,RoleId=admin,Flags=0 /MaAuth/UserSha256/fonwin
/MaAuth/UserSha256/fonwin repw Password(You can use a sentence.)
ss,HomePath=/ /MaAuth/PoAcl/admin
ss,Rights=xff /MaAuth/PoAcl/admin/'/'
ss,Rights=xff /MaAuth/PoAcl/admin/'/..'
```
