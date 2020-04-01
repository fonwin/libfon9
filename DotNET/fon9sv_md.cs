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

      /// 暫緩撮合.
      HeldMatchMask = 0xc0,
      /// 暫緩撮合, 瞬間價格趨漲.
      HeldMatchTrendRise = 0x80,
      /// 暫緩撮合, 瞬間價格趨跌.
      HeldMatchTrendFall = 0x40,
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
   };
}
