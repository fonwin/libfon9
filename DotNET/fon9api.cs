using System;
using System.Text;
using System.Runtime.InteropServices;

namespace fon9
{
   using uint32_t = System.UInt32;

   class Api
   {
      /// 啟動 fon9 C API 函式庫.
      /// - 請使用時注意: 不考慮 multi thread 同時呼叫 fon9_Initialize()/fon9_Finalize();
      /// - 可重覆呼叫 fon9_Initialize(), 但須有對應的 fon9_Finalize();
      ///   最後一個 fon9_Finalize() 被呼叫時, 結束函式庫.
      /// - 啟動 Device factory.
      /// - 啟動 LogFile: 
      ///   - if (logFileFmt == NULL) 表示不改變 log 的輸出位置, 預設 log 輸出在 console.
      ///   - if (*logFileFmt == '\0') 則 logFileFmt = "./logs/{0:f+'L'}/fon9cli.log";
      ///     {0:f+'L'} 表示使用 local time 的 YYYYMMDD.
      /// - 若返回非 0:
      ///   - ENOSYS: 已經呼叫 fon9_Finalize() 系統結束.
      ///   - 其他為 log 檔開啟失敗的原因(errno).
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "fon9_Initialize", CharSet = CharSet.Ansi)]
      public static extern int Initialize([MarshalAs(UnmanagedType.LPStr)] string logFileFmt);
      /// 結束 fon9 函式庫.
      /// - 結束前必須先將所有建立的物件刪除:
      ///   - 例如: f9rc_CreateClientSession() => f9rc_DestroyClientSession();
      /// - 結束系統後, 無法透過 fon9_Initialize() 重啟, 若要重啟, 必須結束程式重新執行.
      /// - 返回 0 表示已結束, 無法再呼叫 fon9_Initialize() 重新啟動.
      /// - 返回 >0 表示還有多少次, 已呼叫 fon9_Initialize(), 但尚未呼叫 fon9_Finalize();
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "fon9_Finalize")]
      public static extern uint32_t Finalize();

      /// 要求使用者從 console 輸入密碼.
      /// buflen 包含 EOS'\0', 一般用法:
      /// 如果 promptFD == NULL: 則不輸出 '*';
      /// 如果 promptStr != NULL: 則: 若 promptFD==NULL 則使用 stderr 輸出;
      /// \return 不含EOS的長度.
      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "fon9_getpass", CharSet = CharSet.Ansi)]
      static extern UInt64 getpass(IntPtr promptFD, string prompt, ref byte pwbuf, UInt64 bufsz);
      public static string getpass(string prompt)
      {
         byte[] pwbuf = new byte[1024];
         UInt64 sz = getpass(IntPtr.Zero, prompt, ref pwbuf[0], (uint)pwbuf.Length);
         return Encoding.UTF8.GetString(pwbuf, 0, (int)sz);
      }
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public struct CStrView
   {
      internal IntPtr Begin_;
      internal IntPtr End_;

      public int Length { get { return (int)(this.End_.ToInt64() - this.Begin_.ToInt64()); } }

      public override string ToString()
      {
         if (this.Begin_ == IntPtr.Zero)
            return string.Empty;
         // 適用於 .NET Core 3.1 3.0 2.2 2.1 2.0 1.1; .NET Standard 2.1;
         // .NET Framework 4.6 未支援
         // return Marshal.PtrToStringUTF8(this.Begin_, this.Length);
         unsafe
         {  // .NET Framework 4.6(含以上) 才有支援 Encoding.UTF8.GetString(byte*, int);
            return Encoding.UTF8.GetString((byte*)this.Begin_.ToPointer(), this.Length);
         }
      }
   }
}
