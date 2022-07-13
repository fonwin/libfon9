namespace f9fmkt
{
   /// \ingroup fmkt
   /// 回報項目種類(下單要求種類).
   /// 用來標註 TradingRxItem 的種類.
   public enum RxKind : ushort
   {
      Unknown = '\0',
      RequestNew = 'N',
      RequestDelete = 'D',
      RequestChgQty = 'C',
      RequestChgPri = 'P',
      RequestQuery = 'Q',
      /// 修改條件單的觸發條件.
      RequestChgCond = 'c',
      /// 成交回報.
      Filled = 'f',
      /// 委託異動.
      Order = 'o',
      /// 系統事件.
      Event = 'e',
   }

   /// \ingroup fmkt
   /// 商品的交易市場.
   /// - 在系統設計時決定市場別, 通常用來區分「可直接連線」的不同交易市場.
   /// - 這裡沒有將所有可能的 market 都定義出來, 當有實際需求時可自行增加.
   public enum TradingMarket : ushort
   {
      Unknown = '\0',
      /// 台灣上市(集中交易市場)
      TwSEC = 'T',
      /// 台灣上櫃(櫃檯買賣中心)
      TwOTC = 'O',
      /// 台灣興櫃.
      TwEMG = 'E',
      /// 台灣期交所-期貨市場.
      TwFUT = 'f',
      /// 台灣期交所-選擇權市場.
      TwOPT = 'o',
   }

   /// \ingroup fmkt
   /// 交易時段代碼.
   /// - FIX 4.x TradingSessionID(#336): 並沒有明確的定義, 由雙邊自訂.
   /// - FIX 5.0 TradingSessionID(#336): http://fixwiki.org/fixwiki/TradingSessionID
   ///   '1'=Day; '2'=HalfDay; '3'=Morning; '4'=Afternoon; '5'=Evening; '6'=AfterHours; '7'=Holiday;
   /// - 台灣期交所 FIX 的 Trading Session Status Message 定義 TradingSessionID=流程群組代碼.
   /// - 台灣證交所 FIX 沒有 Trading Session Status Message,
   ///   但要求提供 TargetSubID(#57)=[TMP協定的 AP-CODE]:
   ///   Trading session: Required for New / Replace / Cancel / Query / Execution
   ///   '0'=Regular Trading; '7'=FixedPrice; '2'=OddLot.
   public enum TradingSessionId : ushort
   {
      Unknown = 0,
      /// 一般(日盤).
      Normal = 'N',
      /// 零股.
      OddLot = 'O',
      /// 盤後定價.
      FixedPrice = 'F',
      /// 盤後交易(夜盤).
      AfterHour = 'A',
   }

   /// 交易時段狀態.
   /// (value & 0x0f) = FIX Tag#340(TradSesStatus:State of the trading session).
   /// 這裡列出可能的狀態, 但實務上不一定會依序, 或確實更新.
   /// - 例: PreClose 狀態, 雖然「台灣證交所」有此狀態, 但系統不一定會提供.
   /// - 必定會異動的情況是:
   ///   - 系統換日清盤(Clear).
   ///   - 收到交易所送出的異動訊息.
   public enum TradingSessionSt : byte
   {
      Unknown = 0x00,
      RequestRejected = 0x06,

      /// 清盤(FIX 無此狀態).
      Clear = 0x10,

      /// 開盤前可收單.
      PreOpen = 0x34,
      /// 不可刪單階段, 通常在開盤前 n 分鐘, 例: 台灣期交所開盤前2分鐘不可刪單.
      NonCancelPeriod = 0x44,

      /// 延後開盤: 例如台灣證交所有提供「試算後延後開盤」.
      DelayOpen = 0x72,
      /// 一般盤中.
      Open = 0x82,
      /// 異常重新開盤, 例如: 期交所發生異常, 期交所會用 SeqReset 訊息通知, 此時 fon9 會用此狀態通知.
      OpenSeqReset = 0x92,

      /// 暫停交易, 可能會恢復, 恢復後的狀態為 Open.
      Halted = 0xb1,
      /// 收盤前, 仍可收單, 但交易方式可能與盤中不同.
      PreClose = 0xe5,

      /// 延後收盤: 例如台灣證交所有提供「試算後延後收盤」.
      DelayClose = 0xe3,
      /// 該時段收盤, 或該時段不再恢復交易.
      Closed = 0xf3,
   }

   public enum PriType : ushort
   {
      Unknown = '\0',
      /// 限價.
      Limit = 'L',
      /// 市價.
      Market = 'M',
      /// 一定範圍內市價.
      Mwp = 'm',
   }

   /// \ingroup fmkt
   /// 時間限制.
   public enum TimeInForce : ushort
   {
      /// 當日有效. ROD, GFD.
      ROD = '\0',
      /// 立即成交否則取消.
      IOC = 'I',
      /// 立即全部成交否則取消.
      FOK = 'F',
      /// 報價單,一定期間後交易所自動刪除.
      QuoteAutoCancel = '8',
   }

   /// \ingroup fmkt
   /// 買賣別.
   public enum Side : ushort
   {
      Unknown = '\0',
      Buy = 'B',
      Sell = 'S',
   }

   /// \ingroup fmkt
   /// 下單要求狀態.
   /// 這裡的值有順序性, Request 的狀態值只會往後增加.
   public enum TradingRequestSt : byte
   {
      /// 建構時的初始值.
      Initialing = 0,

      /// 條件單初步檢查成功, 正在等候條件成立.
      /// 條件成立後, 會繼續走 [風控、送單] 流程, 狀態依序變化.
      WaitingCond = 0x0a,
      /// 在另一台主機等候條件觸發.
      WaitingCondAtOther = 0x0b,

      /// - 當檢查方式需要他方協助, 無法立即得到結果, 則在檢查前設定此狀態.
      /// - 如果檢查方式可立即完成:「失敗拒絕下單 or 檢查通過繼續下一步驟」, 則檢查前可以不用設定此狀態.
      Checking = 0x1c,
      /// 下單要求排隊中.
      Queuing = 0x20,
      /// 下單要求在另一台主機排隊.
      QueuingAtOther = 0x21,

      /// 在呼叫 io.Send() 之前設定的狀態.
      /// 您可以自行決定要在 io.Send() 之前 or 之後 or both, 設定送出狀態.
      Sending = 0x30,
      /// 在呼叫 io.Send() 之後設定的狀態.
      /// 如果要在送出後設定狀態: 則要小心考慮在 io.Send() 之後, 設定 Sent 之前, 就已收到送出的結果.
      Sent = 0x38,

      /// 小於等於此值表示: 處理過程的狀態變化, 表示仍在處理中.
      LastRunStep = 0x9f,

      /// 交易所部分回報: 有些下單要求交易所會有多次回報.
      /// 針對「某筆下單要求」交易所「最後一次回報」前的狀態.
      /// 此部分回報為成功.
      PartExchangeAccepted = 0xa2,
      /// 交易所部分回報: 有些下單要求交易所會有多次回報.
      /// 針對「某筆下單要求」交易所「最後一次回報」前的狀態.
      /// 此部分回報為失敗.
      PartExchangeRejected = 0xae,
      /// 如果確定交易所刪單會分成多次傳送,
      /// 則首次收到的交易所刪單回報, 使用此狀態.
      /// - 例: 報價單 Bid/Offer 分2筆回報, 首次的 Bid 回報使用此狀態;
      ///       Offer 使用 ExchangeCanceled.
      PartExchangeCanceled = 0xa7,

      /// 下單要求流程已結束.
      Done = 0xda,

      // 不明原因結束, 無法確定此要求是否成功.
      // 例如: 要求送出後斷線.
      // ** 移除此狀態 **
      // ** 若送出後沒收到回報, 不論重啟或斷線, 都不改變最後狀態.
      // ** 保留最後狀態, 維運人員較容易研判問題.
      //Broken = 0xdb,

      /// 排隊中的要求被取消, 此筆要求沒送出.
      /// 可能因程式結束後重啟, 或新單排隊中但收到刪單要求.
      QueuingCanceled = 0xe2,
      /// 下單要求因「線路狀況」拒絕. 尚未送給交易所.
      /// 例如: 無可用線路.
      LineRejected = 0xe3,
      /// 下單要求因「委託書號」拒絕. 尚未送給交易所.
      /// 例如: 已無可用櫃號; 自編委託書號重複.
      OrdNoRejected = 0xe4,
      /// 下單要求因「內部其他原因」拒絕. 尚未送給交易所.
      InternalRejected = 0xe9,
      /// 下單要求因「風控檢查」拒絕. 尚未送給交易所.
      CheckingRejected = 0xec,
      /// 下單要求被「交易所」拒絕.
      ExchangeRejected = 0xee,
      /// 刪改要求因「刪改時已無剩餘量,但曾經有此委託」而被「交易所」拒絕刪改.
      /// 請檢查: 是否已全部成交? 或是否已經被刪(交易所刪單、其他系統刪減...)?
      ExchangeNoLeavesQty = 0xef,

      // 新單要求: 尚未收到新單回報, 但先收到成交回報.
      // 如果有「新單未成功」時「暫時保留成交」則無此狀態.
      // ** 移除此狀態 **
      // ** 如果可以用「收到的回報(成交、刪、改、查)」確認新單成功數量,
      // ** 則直接在該回報更新前, 更新「新單要求」的最後狀態=ExchangeAccepted
      // ** 然後再更新「收到的回報(成交、刪、改、查)」.
      //FilledBeforeNew = 0xf1,

      /// 已收到交易所的確認回報: 包含新單回報、刪改回報、查詢回報...
      ExchangeAccepted = 0xf2,
      /// 成交回報要求, 直接使用此狀態.
      Filled = 0xf3,

      // 保留:
      // OrderSt.FullFilled = 0xf4,
      // OrderSt.AsCanceled = 0xf5,
      // = f6,

      /// 如果確定「交易所主動」刪單會分成多次傳送,
      /// 則首次收到的交易所刪單回報, 使用此狀態.
      /// - 例: 期交所狀態 47 => 48;
      ///       則首次的 47(因價格穩定措施刪單) 「交易所主動刪單」回報使用此狀態;
      ///       48(退單時之上限或下限價格) 使用 ExchangeCanceled.
      ExchangeCanceling = 0xf7,
      /// 交易所刪單 IOC, FOK... 若有多次回覆, 則最後一次刪單回覆, 使用此狀態.
      ExchangeCanceled = 0xfa,
      /// ExchangeCanceled 之後, 又收到必須處理的交易所刪單.
      ExchangeCanceled2 = 0xfb,
      ExchangeCanceled3 = 0xfc,

      // 保留: OrderSt.UserCanceled = 0xfd,

      /// 下單要求流程已結束, 但後續又有變動.
      /// 類似 FIX 的 ExecType=D(Restated).
      Restated = 0xfe,

      // 保留: OrderSt.DoneForDay = 0xff,
      // 這裡的狀態碼, 必須與 f9fmkt_OrderSt 一起考慮.
      // 因為新單的 RequestSt 改變時, 通常會拿新單的 RequestSt 直接更新 OrderSt.
   }

   /// \ingroup fmkt
   /// OrderSt 分成 4 階段:
   /// - 新單還在系統裡未送出.
   ///   - Checking:   風控檢查中.
   ///   - Queuing:    排隊中.
   /// - 新單已送出但尚未收到回報.
   ///   - Sending:    傳送中.
   ///   - Sent:       已送出, 但尚未收到交易所回報.
   ///   - Broken:     狀態不明: 送出後,未收到回報,但是斷線了.
   /// - 委託已成立:
   ///   - Accepted:   交易所已收單.
   ///   - PartFilled: 部分成交.
   /// - 委託已結束:
   ///   - 進入此階段後, 狀態就不會再變動.
   ///   - 當本機發現交易所的 WorkQty 變成 0 時.
   ///     - 刪減後的 AfterQty=0: 使用者刪減、交易所刪減(IOC、FOK、超過限額...)
   ///     - 收到成交回報後的 LeavesQty=0
   ///     - 交易所收盤.
   ///     - 要注意的是: 「成交回報、刪減回報」可能會亂序!
   ///       - 假設某筆委託交易所異動順序如下:   
   ///         新10, 減1(Bf=10,Af=9), 成2, 減2(Bf=7,Af=5), 成2, 刪(Bf=3,Af=0)
   ///       - 若分別在不同的主機上操作, 最多可能有 6*5*4*3*2*1=720 種順序組合!!
   ///   - Rejected:   委託被拒絕.
   ///     - BrokerRejected:   券商拒絕.
   ///     - ExchangeRejected: 交易所拒絕.
   ///   - Canceled:   刪單成功(已刪單狀態), 刪減後 AfterQty=0
   ///     - 透過刪減回報的「BeforeQty - AfterQty」來調整 LeavesQty.
   ///     - 但此時本機的 LeavesQty 不一定為 0, 因為可能有「在途成交」或「其他未收到的減量」.
   ///     - 若有在途成交, 則收完成交後, **不會變成全部成交**
   ///     - UserCanceled:     使用者刪單, 或減量成功後的 AfterQty=0;
   ///     - ExchangeCanceled: 交易所刪單(IOC、FOK...).
   ///   - FullFilled: 全部成交.
   ///   - DoneForDay: 已收盤.
   public enum OrderSt : byte
   {
      Initialing = 0,

      /// 此次委託異動的原因是為了記錄: 被保留的亂序回報.
      /// - 委託異動 = 被保留的回報, 例: BeforeQty, AfterQty, ExgTime, ErrCode... 都會寫到「委託異動」.
      /// - 等適當時機再繼續處理, 例:
      ///   - 收到新單回報.
      ///   - Order.LeavesQty==保留的.BeforeQty.
      ReportPending = 1,
      /// 收到交易所的回報, 但此回報已經臭酸了.
      /// - 可能的情況, 例:
      ///   - 查單 => 刪單 => 刪單回報成功 => 查單回報剩餘量3(Stale)
      ///   - 改價A => 改價B => 改價B成功 => 改價A成功(Stale);
      /// - 若無法用數量來判斷, 則使用交易所時間來判斷.
      /// - 若連交易所時間都相同, 則使用收到的先後順序來處理, 所以仍有誤判的可能.
      /// - 此時「委託異動」內容只有底下欄位會變動, 只是為了記錄該筆要求已處理完畢:
      ///   - OrderSt   = ReportStale
      ///   - RequestSt = report st.
      ///   - ErrCode   = report err code.
      ///   - 其他委託內容(例: ExgTime, Qty, Pri...)都不會變動.
      ReportStale = 2,

      WaitingCond = TradingRequestSt.WaitingCond,
      WaitingCondAtOther = TradingRequestSt.WaitingCondAtOther,

      NewStarting = 0x10,
      NewChecking = TradingRequestSt.Checking,
      NewQueuing = TradingRequestSt.Queuing,
      NewQueuingAtOther = TradingRequestSt.QueuingAtOther,
      NewSending = TradingRequestSt.Sending,
      NewSent = TradingRequestSt.Sent,

      NewPartExchangeAccepted = TradingRequestSt.PartExchangeAccepted,
      NewPartExchangeRejected = TradingRequestSt.PartExchangeRejected,
      // = TradingRequestSt.PartExchangeCanceled = 0xa7,

      NewDone = TradingRequestSt.Done,
      //NewBroken               = TradingRequestSt.Broken,

      NewQueuingCanceled = TradingRequestSt.QueuingCanceled,
      NewLineRejected = TradingRequestSt.LineRejected,
      NewOrdNoRejected = TradingRequestSt.OrdNoRejected,
      NewInternalRejected = TradingRequestSt.InternalRejected,
      NewCheckingRejected = TradingRequestSt.CheckingRejected,
      NewExchangeRejected = TradingRequestSt.ExchangeRejected,

      ExchangeAccepted = TradingRequestSt.ExchangeAccepted,
      PartFilled = TradingRequestSt.Filled,
      FullFilled = 0xf4,
      /// 部分成交之後(其實是全部成交), 才收到成交前的減量回報.
      /// - 例: 新單10:Accepted, 成交3:PartFilled, 減量7(Bf=10,Af=3):AsCanceled
      AsCanceled = 0xf5,
      // 保留: = 0xf6,

      /// 若在 OmsErrCodeAct 有設定 St=ExchangeCanceled; 或 ExchangeCanceling;
      /// 則在 OmsRequestRunnerInCore::Update(); 會設定 UpdateOrderSt_ = St;
      /// 此時就會使用到這些狀態.
      Canceling = TradingRequestSt.ExchangeCanceling,
      ExchangeCanceled = TradingRequestSt.ExchangeCanceled,
      ExchangeCanceled2 = TradingRequestSt.ExchangeCanceled2,
      ExchangeCanceled3 = TradingRequestSt.ExchangeCanceled3,
      /// 一般用於使用者的刪單要求成功刪單完成.
      UserCanceled = 0xfd,

      // 保留: = TradingRequestSt.Restated = 0xfe,

      DoneForDay = 0xff,
      // 這裡的狀態碼, 必須與 f9fmkt_TradingRequestSt 一起考慮.
      // 因為新單的 RequestSt 改變時, 通常會拿新單的 RequestSt 直接更新 OrderSt.
   }

   public static class Tools
   {
      static public bool IsFinishedRejected(TradingRequestSt st)
      {
         return ((byte)st & 0xf0) == 0xe0;
      }
      static public bool IsAnyRejected(TradingRequestSt st)
      {
         return IsFinishedRejected(st)
            || st == TradingRequestSt.PartExchangeRejected;
      }
      static public bool IsPart(TradingRequestSt st)
      {
         return ((byte)st & 0xf0) == 0xa0;
      }

      static public bool IsFinishedRejected(OrderSt st)
      {
         return IsFinishedRejected((TradingRequestSt)st);
      }
      static public bool IsAnyRejected(OrderSt st)
      {
         return IsAnyRejected((TradingRequestSt)st);
      }
      static public bool IsCanceled(OrderSt st)
      {
         return OrderSt.AsCanceled <= st && st <= OrderSt.UserCanceled;
      }
   }
}