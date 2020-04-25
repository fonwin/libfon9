using System;
using System.Runtime.InteropServices;

namespace f9sv
{
   using uint8_t = System.Byte;

   public class MdApi
   {
      /// 將 "MdRts" 加入 StreamDecoder; 之後可以使用: "$TabName:MdRts:Args"; 的格式.
      /// - 在 f9sv_Initialize() 之後, f9sv_Subscribe() 之前, 執行一次即可.
      /// - 行情訂閱: 訂閱參數=? 如何回補? 如何區分日夜盤?
      ///   回補應可到「前一盤」, 不論前一盤是日盤或夜盤.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9sv_AddStreamDecoder_MdRts")]
      public static extern ResultCode AddStreamDecoder_MdRts();
   }

   [Flags]
   public enum DealFlag : uint8_t
   {
      /// 試撮價格訊息.
      /// 何時可能會有試撮? 開盤前, 收盤前, 瞬間漲跌幅過大暫停交易期間(保險絲熔斷機制)...
      Calculated = 0x01,

      /// 成交時間有異動.
      DealTimeChanged = 0x02,
      /// DealBuyCnt 有異動.
      DealBuyCntChanged = 0x04,
      /// DealSellCnt 有異動.
      DealSellCntChanged = 0x08,

      /// TotalQty 不連續, 藉此判斷是否有遺漏成交明細.
      TotalQtyLost = 0x10,

      /// LmtFlags 欄位有異動.
      LmtFlagsChanged = 0x20,
   }

   /// 這裡的定義與台灣證交所 Fmt6 的「漲跌停註記」欄位的 bit 位置相同.
   [Flags]
   public enum DealLmtFlag : uint8_t
   {
      /// 一般非漲跌停成交.
      None = 0x00,
      /// 漲停成交.
      UpLmt = 0x80,
      /// 跌停成交.
      DnLmt = 0x40,
      /// 暫緩撮合.
      HeldMatchMask = 0x03,
      /// 暫緩撮合, 瞬間價格趨漲.
      HeldMatchTrendRise = 0x02,
      /// 暫緩撮合, 瞬間價格趨跌.
      HeldMatchTrendFall = 0x01,
   }

   [Flags]
   public enum BSFlag : uint8_t
   {
      /// 試撮後剩餘委託簿.
      Calculated = 0x01,
      /// 委託簿的買方有異動.
      OrderBuy = 0x02,
      /// 委託簿的賣方有異動.
      OrderSell = 0x04,
      /// 委託簿的衍生買方有異動.
      DerivedBuy = 0x10,
      /// 委託簿的衍生賣方有異動.
      DerivedSell = 0x20,
   }

   /// 這裡的定義與台灣證交所 Fmt6 的「漲跌停註記」欄位的 bit 位置相同.
   [Flags]
   public enum BSLmtFlag : uint8_t
   {
      /// 最佳一檔買進 = 漲停.
      UpLmtBuy = 0x20,
      /// 最佳一檔買進 = 跌停.
      DnLmtBuy = 0x10,
      /// 最佳一檔賣出 = 漲停.
      UpLmtSell = 0x08,
      /// 最佳一檔賣出 = 跌停.
      DnLmtSell = 0x04,
   };

   public enum TradingSessionId : byte
   {
      Unknown = 0,
      /// 一般(日盤).
      Normal = (byte)'N',
      /// 零股.
      OddLot = (byte)'O',
      /// 盤後定價.
      FixedPrice = (byte)'F',
      /// 盤後交易(夜盤).
      AfterHour = (byte)'A',
   }

   /// 交易時段狀態.
   /// (value & 0x0f) = FIX Tag#340(TradSesStatus:State of the trading session).
   /// 這裡列出可能的狀態, 但實務上不一定會依序, 或確實更新.
   /// - 例: PreClose 狀態, 雖然「台灣證交所」有此狀態, 但系統不一定會提供.
   /// - 必定會異動的情況是:
   ///   - 系統換日清盤(Clear).
   ///   - 收到交易所送出的異動訊息.
   public enum TradingSessionSt : uint8_t
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

      /// 暫停交易, 可能會恢復, 恢復後的狀態為 Open.
      Halted = 0xb1,
      /// 收盤前, 仍可收單, 但交易方式可能與盤中不同.
      PreClose = 0xe5,

      /// 延後收盤: 例如台灣證交所有提供「試算後延後收盤」.
      DelayClose = 0xe3,
      /// 該時段收盤, 或該時段不再恢復交易.
      Closed = 0xf3,
   }

   public enum RtsPackType : uint8_t
   {
      /// 打包成交明細, 一個 DealPack 可能觸發多次「交易明細」異動事件.
      DealPack,

      /// 委託簿快照(買賣報價).
      SnapshotBS,
      /// 試算委託簿快照.
      CalculatedBS,
      /// 委託簿異動.
      UpdateBS,

      TradingSessionId,

      BaseInfoTw,

      /// DealPack(有InfoTime) + SnapshotBS(無InfoTime);
      /// 使用 f9fmkt::DealFlag::Calculated 判斷是否為試算.
      DealBS,

      SnapshotSymbList_NoInfoTime,
      IndexValueV2,
      IndexValueV2List,
      FieldValue_NoInfoTime,
      FieldValue_AndInfoTime,
      TabValues_NoInfoTime,
      TabValues_AndInfoTime,
   }
}
