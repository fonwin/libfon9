namespace f9io
{
   public enum State
   {
      /// 初始化尚未完成: Device 建構之後的初始狀態.
      Initializing,
      /// 建構 Device 的人必須主動呼叫 Device::Initialize() 才會進入此狀態.
      Initialized,

      /// Device::OpImpl_Open() 因設定錯誤的開啟失敗。
      /// 無法使用相同 cfgstr 再次開啟.
      ConfigError,
      /// 開啟中: 開啟尚未完成.
      /// 若開啟完成, 則進入底下3種可能的狀態, 視不同的 Device Style 而定:
      /// - WaitingLinkIn
      /// - Linking
      /// - Listening
      Opening,

      /// 連入等候中: 例如一個 USB port, 正在等候 USB 設備的插入.
      WaitingLinkIn,
      /// 連線建立中: 例如 TcpClient 呼叫了 connect().
      Linking,
      /// 連線建立失敗: 例如 TcpClient 呼叫 connect() 之後沒有連線成功.
      LinkError,
      /// 連線成功: 可以開始收送資料.
      /// - WaitingLinkIn, Linking 之後, 連線成功的狀態.
      LinkReady,
      /// 連線中斷.
      /// - **LinkReady 之後** , 連線中斷了!
      LinkBroken,

      /// Listening(例如: TcpServer)
      Listening,
      /// Listening之後被中斷: reset 網卡/IP?
      ListenBroken,

      /// 逗留中, 等資料送完後關閉.
      Lingering,
      /// 關閉中.
      Closing,
      /// 已關閉.
      Closed,

      /// 即將解構, Device 已不可再使用!
      /// 當收到此狀態, 您應該盡快將 Device 釋放.
      Disposing,
      /// 在 Device::~Device() 最後的解構狀態.
      Destructing,
   }
}
