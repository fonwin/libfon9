using System;
using System.Runtime.InteropServices;

namespace f9sv
{
   using uint8_t = System.Byte;
   using uint16_t = System.UInt16;
   using uint32_t = System.UInt32;
   using int32_t = System.Int32;
   using uint64_t = System.UInt64;
   using int64_t = System.Int64;
   /// 小數位數.
   using DecScale = System.Byte;
   using TabSize = System.UInt32;
   using SvIndex = System.Int32;

   /// fon9 SeedVisitor 的基本命名物件.
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct Named
   {
      public fon9.CStrView Name_;
      public fon9.CStrView Title_;
      public fon9.CStrView Description_;
      public SvIndex Index_;
      UInt32 Padding___;
   }

   /// 欄位型別.
   public enum FieldType : uint8_t
   {
      /// 類型不明, 或自訂欄位.
      Unknown,
      /// bytes 陣列.
      /// 顯示字串 = 需要額外轉換(例: HEX, Base64...)
      Bytes,
      /// 字元串(尾端不一定會有EOS).
      /// 顯示字串 = 直接使用內含字元, 直到EOS or 最大長度.
      Chars,
      /// 整數欄位.
      Integer = 10,
      /// 固定小數位欄位: fon9::Decimal
      Decimal,
      /// 時間戳.
      TimeStamp,
      /// 時間間隔.
      TimeInterval,
      /// 時間: 距離 00:00:00 的時間間隔.
      /// 輸出格式 `days-hh:mm:ss.uuuuuu`
      DayTime,
   }

   /// fon9 SeedVisitor 的欄位型別.
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)] 
   public struct Field
   {
      public Named Named_;
      IntPtr InternalOwner_;
      /// 欄位型別.
      /// - Cn = char[n], 若 n==0 則為 Blob.Chars 欄位.
      /// - Bn = byte[n], 若 n==0 則為 Blob.Bytes 欄位.
      /// - Un[.s] = Unsigned, n=bytes(1,2,4,8), s=scale(小數位數)可為0
      /// - Sn[.s] = Signed, n=bytes(1,2,4,8), s=scale(小數位數)可為0
      /// - Unx = Unsigned, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
      /// - Snx = Signed, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
      /// - Ti = TimeInterval
      /// - Ts = TimeStamp
      /// - Td = DayTime
      public unsafe fixed byte TypeId_[22];

      public DecScale DecScale_;
      public FieldType Type_;
      /// 欄位資料的大小.
      public uint32_t Size_;
      /// ((const char*)(seed) + Offset_) = 儲存原始資料的位置.
      /// 如果您確定要取得的是整數型內容, 可藉由此方式快速取得, 但請注意記憶體對齊問題.
      public int32_t Offset_;
      /// 若為數值類型的欄位,
      /// 透過 f9sv_GetField_sNN() 或 f9sv_GetField_uNN() 取得的數字, 若與這裡相同, 則表示該值為 null;
      /// - 若為 f9sv_FieldType_Decimal:
      ///   - Unsigned_ = max uint64_t or uint32_t or uint16_t or uint8_t;
      ///   - Signed_   = min int64_t or int32_t or int16_t or int8_t;
      /// - 若為整數, 則為 0;
      public NumFieldNullValue NullValue_;
   }
   [StructLayout(LayoutKind.Explicit)]
   public struct NumFieldNullValue
   {
      [FieldOffset(0)]
      public uint64_t Unsigned_;
      [FieldOffset(0)]
      public int64_t Signed_;
   }

   /// fon9 seed 機制, 每個 key 可能對應數種資料, 使用 Tab 分類資料,
   /// 例如: 某商品Id, 可能有:
   /// - Tab="Base": 基本資料(名稱、市場別...)
   /// - Tab="Ref":  今日參考價(平盤價、漲停價、跌停價...)
   /// - Tab="Deal": 成交價量(成交時間、成交價、成交量...)
   /// - Tab="BS":   買賣報價
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct Tab {
      public Named Named_;
      public unsafe Field* FieldArray_;
      public uint32_t FieldCount_;
      UInt32 Padding___;

      public unsafe Field* FindField(string fldName)
      {
         return FindField(System.Text.ASCIIEncoding.ASCII.GetBytes(fldName));
      }
      public unsafe Field* FindField(byte[] pNameBuf)
      {
         void* pName = Marshal.UnsafeAddrOfPinnedArrayElement(pNameBuf, 0).ToPointer();
         f9sv.Field* pfld = this.FieldArray_;
         for (uint count = this.FieldCount_; count > 0; --count)
         {
            if (pfld->Named_.Name_.Length == pNameBuf.Length
               && Api.memcmp(pfld->Named_.Name_.Begin_.ToPointer(), pName, pNameBuf.Length) == 0)
               return pfld;
            ++pfld;
         }
         return null;
      }
   }
   /// Tab 集中起來, 構成一個 Layout, 用來描述一筆完整的資料.
   /// - 使用 layout 來描述 pod 的資料格式.
   /// - pod 裡面包含多個 seed, 使用 layout 裡面的 tab 來描述一個 seed;
   /// - tree 為 pod 的容器: 一棵 tree 包含 0..n 個 pod;
   /// - pod 為 seed 的容器: 一個 pod 包含 1..n 個 seed;
   /// - seed 可能會有 0..1 棵 sapling(子樹), 是否會有 sapling? 怎樣才有 sapling? 由 seed 決定;
   ///   - 例如: "台股商品/2330^Deal" = 成交明細資料表.
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct Layout
   {
      public Field KeyField_;
      /// 指向 Tab 的陣列.
      public unsafe Tab* TabArray_;
      public TabSize TabCount_;

      public unsafe Tab* FindTab(string tabName)
      {
         byte[] pNameBuf = System.Text.ASCIIEncoding.ASCII.GetBytes(tabName);
         void* pName = Marshal.UnsafeAddrOfPinnedArrayElement(pNameBuf, 0).ToPointer();
         f9sv.Tab* ptab = this.TabArray_;
         for (uint count = this.TabCount_; count > 0; --count)
         {
            if (ptab->Named_.Name_.Length == pNameBuf.Length
               && Api.memcmp(ptab->Named_.Name_.Begin_.ToPointer(), pName, pNameBuf.Length) == 0)
               return ptab;
            ++ptab;
         }
         return null;
      }
   }

   /// 提供一個 table 裡面的資料(使用字串).
   /// 通常用在登入成功時, Server 端提供的相關訊息.
   /// 例如: OMS使用者的: 可用帳號列表、可用櫃號列表、流量管制參數...
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct GvList
   {
      // IntPtr = Field*
      IntPtr FieldArray_;
      uint32_t FieldCount_;
      uint32_t RecordCount_;
      unsafe fon9.CStrView** RecordList_;
   }
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct GvTable
   {
      Named Named_;
      GvList GvList_;
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct GvTables
   {
      public fon9.CStrView OrigStrView_;
      unsafe GvTable** TableList_;
      uint32_t TableCount_;
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct ClientConfig
   {
      /// - TableName = PoAcl
      ///   - Fields = Path, Rights, MaxSubrCount
      ///   - Rights = fon9/seed/SeedAcl.hpp: enum class AccessRight;
      public GvTables RightsTables_;

      /// 查詢流量管制.
      public uint16_t FcQryCount_;
      public uint16_t FcQryMS_;
      /// 合計最多可訂閱數量.
      public uint32_t MaxSubrCount_;
   }
   public delegate void FnOnConfig(ref f9rc.RcClientSession ses, ref ClientConfig cfg);

   [StructLayout(LayoutKind.Sequential)]
   public class ClientSessionParams : f9rc.FunctionNoteParams
   {
      public ClientSessionParams() : base(f9rc.FunctionCode.SeedVisitor)
      {
      }
      /// 通知時機:
      /// - 當登入成功後, Server 回覆 tree list.
      /// - 您可以在收到此事件時「訂閱」需要的項目.
      public FnOnConfig FnOnConfig_;
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct SeedName
   {
      [MarshalAs(UnmanagedType.LPStr)]
      public string TreePath_;
      [MarshalAs(UnmanagedType.LPStr)]
      public string SeedKey_;
      /// 若有提供 TabName_ (!= null), 則不理會 TabIndex_;
      /// - 查詢可使用 "*" 表示查詢全部的 tabs.
      /// - 訂閱、取消訂閱不支援 "*".
      [MarshalAs(UnmanagedType.LPStr)]
      public string TabName_;
      public TabSize TabIndex_;
   }

   /// 收到訂閱或查詢的資料.
   [StructLayout(LayoutKind.Sequential)]
   public struct ClientReport
   {
      public ResultCode ResultCode_;
      /// Stream 回報時提供, 由 Stream 定義內容.
      public uint16_t StreamPackType_;
      uint16_t Padding___;
      /// 使用者訂閱時自訂的編號.
      public IntPtr UserData_;
      /// 查詢或訂閱時提供的 TreePath (已正規化: 移除多餘的 '/', '.' 之類).
      public fon9.CStrView TreePath_;
      /// 不論 Server 端的 key 為何種型別, 回報事件都會提供字串.
      public fon9.CStrView SeedKey_;
      /// TreePath 的資料格式.
      public unsafe Layout* Layout_;
      /// 此次回報的是哪個 Tab* 的資料.
      public unsafe Tab* Tab_;
      /// 可透過 f9sv.GetField_*() 取得欄位內容.
      public IntPtr Seed_;
      /// 由事件發行者提供, 其他 seeds 的資料.
      /// - 例如: 某次「成交明細」異動, 此處可能提供該商品全部的資料,
      ///   - 此時可用 SeedArray_[bsTabIdx] 取得「上次買賣報價」的內容.
      ///   - 取得 Tab 的方式: (Tab_ - Tab_->Named_.Index_)[bsTabIdx];
      public IntPtr SeedArray_;
      /// 由回報提供者額外提供的識別資料.
      /// 例如:
      ///   - ((f9sv.RtsPackType)StreamPackType_) ==
      ///      f9sv.RtsPackType.FieldValue_NoInfoTime,
      ///      f9sv.RtsPackType.FieldValue_AndInfoTime;
      ///     - 則此處為 Int32* fields = ((Int32*)StreamPackExArgs_);
      ///     - N = fields[0] = 異動的欄位數量;
      ///     - fields[1..N] = 異動的欄位索引.
      public IntPtr StreamPackExArgs_;

      public unsafe IntPtr IndexSeed(int idx)
      {
         if (this.SeedArray_ == IntPtr.Zero)
            return IntPtr.Zero;
         return ((IntPtr*)this.SeedArray_.ToPointer())[idx];
      }
   }

   /// 通知收到 SeedVisitor 的: 查詢結果、訂閱結果、訂閱訊息...
   /// - 在通知前會鎖定 session, 您應該盡快結束事件處理, 且不建議在此等候其他 thread.
   /// - 尤其不能等候其他 thread 處理「f9sv_ API 呼叫」, 這樣會造成死結.
   public delegate void FnOnReport(ref f9rc.RcClientSession session, ref ClientReport rpt);

   /// 呼叫「查詢、訂閱、取消訂閱」時, 提供的結果通知物件.
   [StructLayout(LayoutKind.Sequential)]
   public struct ReportHandler
   {
      public FnOnReport FnOnReport_;
      public IntPtr UserData_;
      public ReportHandler(FnOnReport fnOnReport, IntPtr userData)
      {
         this.FnOnReport_ = fnOnReport;
         this.UserData_ = userData;
      }
   }

   public static class Api
   {
      /// 啟動 f9sv(fon9 SeedVisitor Rc client) 函式庫.
      /// - 請使用時注意: 禁止 multi thread 同時呼叫 f9sv_Initialize()/fon9_Finalize();
      /// - 可重覆呼叫 f9sv_Initialize(), 但須有對應的 fon9_Finalize();
      ///   最後一個 fon9_Finalize() 被呼叫時, 結束函式庫.
      /// - 其餘請參考 fon9_Initialize();
      /// - 然後透過 f9rc_CreateClientSession() 建立 Session;
      ///   - 建立 Session 時, 必須提供 f9sv_ClientSessionParams; 參數.
      ///   - 且需要經過 f9sv_InitClientSessionParams() 初始化.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9sv_Initialize", CharSet = CharSet.Ansi)]
      public static extern int Initialize([MarshalAs(UnmanagedType.LPStr)] string logFileFmt);

      /// 送出查詢訊息.
      /// 查詢結果不一定會依照查詢順序回報.
      /// \retval ==f9sv_Result_NoError 查詢要求已送出(或因尚未取得 Layout 而暫時保留).
      ///            如果網路查詢返回夠快, 返回前可能已透過 FnOnReport_() 通知查詢結果(可能成功 or 失敗).
      /// \retval <f9sv_Result_NoError 無法查詢, 失敗原因請參考 f9sv_Result;
      ///            會先用 FnOnReport_() 通知查詢失敗, 然後才返回.
      /// \retval >f9sv_Result_NoError 流量管制, 需要等 ((unsigned)retval) ms 之後才能再查詢.
      ///            會先用 FnOnReport_() 通知查詢失敗, 然後才返回.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9sv_Query", CharSet = CharSet.Ansi)]
      unsafe extern static ResultCode Query(f9rc.RcClientSession* ses, ref SeedName seedName, ReportHandler handler);
      unsafe public static ResultCode Query(f9rc.ClientSession ses, ref SeedName seedName, ReportHandler handler)
         => Query(ses.RcSes_, ref seedName, handler);

      /// 訂閱要求.
      /// 訂閱通知流程:
      /// - f9sv_Subscribe(): rpt.Seed_ == NULL;
      ///   - 成功: rpt.ResultCode_ == f9sv_Result_NoError;
      ///   - 失敗: rpt.ResultCode_ ＜ f9sv_Result_NoError;
      /// - 收到訂閱的資料:
      ///   - rpt.Seed_ != NULL;
      ///   - rpt.ResultCode_ == f9sv_Result_NoError;
      /// - f9sv_Unsubscribe(): rpt.Seed_ == NULL;
      ///   - 使用訂閱時的 regHandler 通知: rpt.ResultCode_ == f9sv_Result_Unsubscribing;
      ///   - 之後不會再使用訂閱時的 regHandler 通知, 改成使用「取消訂閱時的 unregHandler」通知.
      ///   - 若在取消訂閱成功之前, 還有「收到訂閱的資料」, 則會使用「取消訂閱時的 unregHandler」通知.
      /// - 訂閱 Stream 訊息時, 必須用 seedName->TabName_ = "$TabName:StreamDecoderName:Args"; 的格式.
      ///
      /// \retval >=f9sv_Result_NoError, 訂閱要求已送出.
      ///            如果網路返回夠快, 返回前可能已透過 FnOnReport_() 通知訂閱結果及訊息.
      /// \retval ＜f9sv_Result_NoError 無法訂閱, 失敗原因請參考 f9sv_Result;
      ///            會先用 FnOnReport_() 通知訂閱失敗, 然後才返回.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9sv_Subscribe", CharSet = CharSet.Ansi)]
      unsafe static extern ResultCode Subscribe(f9rc.RcClientSession* ses, ref SeedName seedName, ReportHandler regHandler);
      unsafe static public ResultCode Subscribe(f9rc.ClientSession ses, ref SeedName seedName, ReportHandler regHandler)
         => Subscribe(ses.RcSes_, ref seedName, regHandler);

      /// 取消訂閱.
      /// 若在取消訂閱成功之前, 還有「收到訂閱的資料」, 則會使用「取消訂閱時的 unregHandler」通知.
      ///
      /// \retval ==f9sv_Result_NoError 取消訂閱要求已送出.
      ///            - 返回前會先使用「訂閱時的 regHandler」通知 f9sv_Result_Unsubscribing;
      ///            - 如果網路返回夠快, 返回前可能已透過 unregHandler 通知「訂閱已取消 f9sv_Result_Unsubscribed」.
      /// \retval ==f9sv_Result_Unsubscribed 已取消「尚未送出的訂閱要求」.
      ///            - 返回前會先使用「訂閱時的 regHandler」通知 f9sv_Result_Unsubscribing;
      ///            - 返回前已透過 unregHandler 通知「訂閱已取消 f9sv_Result_Unsubscribed」.
      /// \retval ＜f9sv_Result_NoError  無法取消訂閱, 失敗原因請參考 f9sv_Result;
      ///            會先用 unregHandler 通知取消失敗, 然後才返回.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9sv_Unsubscribe", CharSet = CharSet.Ansi)]
      unsafe static extern ResultCode Unsubscribe(f9rc.RcClientSession* ses, ref SeedName seedName, ReportHandler unregHandler);
      unsafe static public ResultCode Unsubscribe(f9rc.ClientSession ses, ref SeedName seedName, ReportHandler unregHandler)
         => Unsubscribe(ses.RcSes_, ref seedName, unregHandler);

      /// 取得 res 的文字說明.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9sv_GetSvResultMessage", CharSet = CharSet.Ansi)]
      static extern IntPtr f9sv_GetSvResultMessage(ResultCode res);
      static public string GetSvResultMessage(ResultCode res)
         => Marshal.PtrToStringAnsi(f9sv_GetSvResultMessage(res));

      /// 將欄位轉成文字, 尾端填入 EOS, 若緩衝區不足, 則僅顯示部分內容.
      /// *bufsz 傳回包含 EOS 需要的資料量.
      /// 返回 outbuf pointer;
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9sv_GetField_Str", CharSet = CharSet.Ansi)]
      public static extern IntPtr GetField_Str(IntPtr seedmem, ref Field fld, ref byte outbuf, ref uint bufsz);
      public static string GetField_Str(IntPtr seedmem, ref Field fld)
      {
         byte[] buf = new byte[1024];
         uint sz = (uint)buf.Length;
         f9sv.Api.GetField_Str(seedmem, ref fld, ref buf[0], ref sz);
         return System.Text.Encoding.UTF8.GetString(buf, 0, (int)sz - 1/*EOS*/);
      }

      public static UInt64 ReadUnsigned(IntPtr seedmem, ref Field fld)
         => ReadUnsigned(IntPtr.Add(seedmem, fld.Offset_), fld.Size_);
      public static unsafe UInt64 ReadUnsigned(IntPtr pval, uint sz)
      {
         byte* uptr = ((byte*)pval) + sz;
         UInt64 uval = *--uptr;
         for (--sz; sz > 0; --sz)
            uval = (uval << 8) | *--uptr;
         return uval;
      }

      public static Int64 ReadSigned(IntPtr seedmem, ref Field fld)
         => ReadSigned(IntPtr.Add(seedmem, fld.Offset_), fld.Size_);
      public static unsafe Int64 ReadSigned(IntPtr pval, uint sz)
      {
         byte* uptr = ((byte*)pval) + sz;
         Int64 sval = (sbyte)*--uptr;
         for (--sz; sz > 0; --sz)
            sval = (sval << 8) | *--uptr;
         return sval;
      }

      [DllImport("msvcrt.dll", CallingConvention = CallingConvention.Cdecl)]
      internal static extern unsafe int memcmp(void* b1, void* b2, int count);
   }

   public enum ResultCode
   {
      NoError = 0,

      /// 呼叫 f9sv_Unsubscribe() 時, 通知 regHandler 訂閱取消中.
      /// 告知 regHandler: 後續的訂閱資料及取消結果由 unregHandler 接手.
      Unsubscribing = 9641,
      /// f9sv_Unsubscribe() 成功的通知.
      Unsubscribed = 9645,

      /// 訂閱 tree, 收到 NotifyKind = PodRemoved 通知.
      /// 此時應移除該 pod.
      SubrNotifyPodRemoved = 9652,
      /// 訂閱 tree, 收到 NotifyKind = SeedRemoved 通知.
      /// 此時應移除該 seed.
      SubrNotifySeedRemoved = 9653,
      /// 收到 NotifyKind = TableChanged 時.
      /// 此時應清除訂閱的資料表內容, 通常會立即回覆新的資料表.
      SubrNotifyTableChanged = 9654,
      /// - 收到 NotifyKind = ParentSeedClear 通知.
      /// - 訂閱 seed, 收到 NotifyKind = PodRemoved 或 SeedRemoved 通知.
      SubrNotifyUnsubscribed = 9655,

      /// 收到 NotifyKind = SubscribeStreamOK 通知.
      SubrStreamOK = 9656,
      /// 收到 NotifyKind = StreamRecover 通知.
      /// - 一般訂閱(SeedChanged) 及 Stream訂閱(StreamData),
      ///   回報時使用 NoError;
      SubrStreamRecover = 9658,
      /// 收到 NotifyKind = StreamRecoverEnd 通知.
      /// 如果回補完後, Stream 已確定不會再有訊息,
      /// 則會直接回覆 StreamEnd, 不會有 StreamRecoverEnd;
      SubrStreamRecoverEnd = 9659,
      /// 收到 NotifyKind = StreamEnd 通知.
      /// 收到後會自動移除訂閱, 不會再收到該 Stream 的訊息.
      SubrStreamEnd = 9660,

      /// = fon9::seed::OpResult::access_denied = -2,
      AccessDenied = -2,
      /// = fon9::seed::OpResult::not_found_tab = -201,
      NotFoundTab = -201,

      /// 在呼叫 f9sv_Query(); 或 f9sv_Subscribe(); 時,
      /// - 沒有呼叫過 f9sv.Api.Initialize();
      /// - 或建立 ses 時, 沒有提供 f9sv.ClientSessionParams 參數.
      BadInitParams = -10,

      /// 連線中斷. 查詢、訂閱, 都會收到此回報, 然後清除所有的要求(查詢、訂閱...).
      LinkBroken = -11,

      /// 在呼叫 f9sv_Query(); 時, 若有流量管制.
      /// - 先透過 FnOnReport_(FlowControl) 告知查詢失敗.
      /// - 然後 f9sv_Query(); 返回>0, 告知應延遲多少 ms;
      FlowControl = -12,

      /// f9sv_Query() 正在查詢, 尚未取得結果, 則後續相同的 seedName 直接返回失敗. 
      InQuery = -13,

      /// 超過可訂閱數量.
      /// 在收到 Server 端的「取消訂閱確認」之後, 才會扣除訂閱數量.
      /// 目前: 僅支援「Session累計訂閱數量」限制, 沒有檢查「個別 tree 的訂閱數量」.
      OverMaxSubrCount = -20,

      /// 已經有訂閱, 則後續的 f9sv_Query()/f9sv_Subscribe() 直接返回失敗.
      InSubscribe = -21,
      /// 正在取消訂閱, 必須等取消成功後, 才可再次執行 f9sv_Query()/f9sv_Subscribe(), 直接返回失敗.
      /// 或 Server 執行訂閱處理時, 收到取消.
      InUnsubscribe = -22,
   }
}
