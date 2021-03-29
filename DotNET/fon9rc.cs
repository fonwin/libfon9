using System;
using System.Runtime.InteropServices;

namespace f9rc
{
   using uint8_t = System.Byte;
   using uint16_t = System.UInt16;
   using uint32_t = System.UInt32;
   using int32_t = System.Int32;

   [Flags]
   public enum RcFlag : uint16_t
   {
      NoChecksum = 0x01,
   }

   public enum FunctionCode : uint8_t
   {
      Connection = 0,
      SASL = 1,
      Logout = 2,
      Heartbeat = 3,

      SeedVisitor = 6,
      OmsApi = 7,

      /// 這裡提供一個暫時的最大值, 可減少一些記憶體用量.
      /// 目前規劃:
      /// - admin 功能(登入、Heartbeat...)
      /// - SeedVisitor
      /// - OmsRc
      /// - 其他請參考 fon9/rc/README.md
      Count = 8,
   }

   /// 需要記錄那些 log.
   [Flags]
   public enum ClientLogFlag : uint32_t
   {
      None = 0,
      All = 0xffff,

      /// 連線(斷線)相關事件.
      Link = 0x0001,
      AllRcSession = 0x0007,

      AllSeedVisitor   = 0x00f8,
      SvAll            = AllSeedVisitor,
      SvConfig         = 0x0008,
      SvQuery          = 0x0010,
      SvQueryData      = 0x0020,
      SvSubscribe      = 0x0040,
      SvSubscribeData  = 0x0080,

      AllOmsRc    = 0x0f00,
      OmsAll      = AllOmsRc,
      OmsConfig   = 0x0400,
      OmsRequest  = 0x0100,
      OmsReport   = 0x0200,
   }

   public class ClientSession : IDisposable
   {
      internal unsafe RcClientSession* RcSes_;
      GCHandle UDHandle_;

      /// 使用 CloseWait(); 關閉連線.
      public void Dispose()
      {
         this.CloseWait();
      }
      public void CloseWait() => Close(true);
      public void CloseNoWait() => Close(false);
      /// 關閉 rc 連線物件.
      /// 在您撰寫「事件處理函式」時應注意: 是否正在等候銷毀.
      /// 如果您是在「事件處理函式」中呼叫, 則不能等候銷毀(會造成死結).
      /// isWait = true  表示要等確定銷毀後才返回, 在返回前, 仍然可能在其他 thread 收到事件.
      /// isWait = false 表示在返回後仍可能收到事件, 使用此方式關閉連線, 「事件處理函式」必須仔細考慮 session 是否有效.
      public void Close(bool isWait)
      {
         unsafe
         {
            RcClientSession* ses = this.RcSes_;
            if (ses == null)
               return;
            this.RcSes_ = null;
            ses->UserData_ = IntPtr.Zero;
            this.UDHandle_.Free();
            DestroyClientSession(ses, isWait ? 1 : 0);
         }
      }

      /// 開啟一個 rc 連線物件.
      /// - 當您不再使用時, 應呼叫 Close(); 或 CloseWait(); 或 CloseNoWait(); 關閉.
      /// - 開啟之前, 會先用 this.CloseWait(); 關閉之前的連線.
      /// - 除了 args.UserData_ 保留, 由 ClientSession 使用.
      /// - 您必須先填妥 args.UserId_; args.Password_; args.DevName_; args.DevParams_;
      /// - args.DevName_: 通訊設備名稱, 例如: "TcpClient";
      /// - args.DevParams_: 通訊設備參數, 例如: "192.168.1.1:6601"; "dn=localhost:6601"
      /// - 有可能在返回前就觸發事件.
      /// - retval 1 成功.
      /// - retval 0 失敗: devName 不正確, 或尚未呼叫 fon9_Initialize(); 或 f9sv_Initialize();
      public int Open(ClientSessionParams args)
      {
         this.CloseWait();
         this.UDHandle_ = GCHandle.Alloc(this);
         args.Val_.UserData_ = (IntPtr)this.UDHandle_;
         unsafe { return CreateClientSession(out this.RcSes_, args.Val_); }
      }

      unsafe public ClientLogFlag LogFlags
      {
         get { return this.RcSes_ != null ? this.RcSes_->LogFlags_ : ClientLogFlag.None; }
         set { if(this.RcSes_ != null) this.RcSes_->LogFlags_ = value; }
      }

      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9rc_CreateClientSession")]
      unsafe static extern int CreateClientSession(out RcClientSession* result, ClientSessionParamsC args);

      [DllImport(fon9.DotNetApi.kDllName, EntryPoint = "f9rc_DestroyClientSession")]
      unsafe static extern void DestroyClientSession(RcClientSession* ses, int isWait);
   }

   [StructLayout(LayoutKind.Sequential)]
   public struct RcClientSession
   {
      internal IntPtr UserData_;
      public ClientLogFlag LogFlags_;

      public ClientSession GetClientSession()
      {
         if (this.UserData_ == IntPtr.Zero)
            return null;
         return ((GCHandle)this.UserData_).Target as ClientSession;
      }
   }

   [StructLayout(LayoutKind.Sequential)]
   public class FunctionNoteParams
   {
      public readonly int32_t ParamSize_;
      public readonly FunctionCode FunctionCode_;
      public FunctionNoteParams(FunctionCode fnc)
      {
         this.ParamSize_ = Marshal.SizeOf(this);
         this.FunctionCode_ = fnc;
      }
   }
   public delegate void FnOnLinkEv(ref RcClientSession ses, f9io.State st, fon9.CStrView info);

   /// 建立 RcClientSession 連線時所需要的參數.
   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   class ClientSessionParamsC
   {
      [MarshalAs(UnmanagedType.LPStr)]
      public string UserId_;
      [MarshalAs(UnmanagedType.LPStr)]
      public string Password_;
      [MarshalAs(UnmanagedType.LPStr)]
      public string DevName_;
      [MarshalAs(UnmanagedType.LPStr)]
      public string DevParams_;

      public IntPtr UserData_;
      public RcFlag RcFlags_;

      char Reserved1_;
      char Reserved2_;

      public ClientLogFlag LogFlags_;

      /// 連線有關的事件: 連線失敗, 連線後斷線...
      public FnOnLinkEv FnOnLinkEv_;

      /// 各個 FunctionCode 所需要的參數.
      /// FunctionNoteParams* FunctionNoteParams_[(int)FunctionCode.Count];
      [MarshalAs(UnmanagedType.ByValArray, SizeConst = (int)FunctionCode.Count)]
      internal IntPtr[] FunctionNoteParams_ = new IntPtr[(int)FunctionCode.Count];
   }

   [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
   public class ClientSessionParams : IDisposable
   {
      delegate void DestroyParams(IntPtr p);
      DestroyParams[] ParamsDestructor_ = new DestroyParams[(int)FunctionCode.Count];
      internal ClientSessionParamsC Val_ = new ClientSessionParamsC();

      public string UserId_ { get { return this.Val_.UserId_; } set { this.Val_.UserId_ = value; } }
      public string Password_ { get { return this.Val_.Password_; } set { this.Val_.Password_ = value; } }
      public string DevName_ { get { return this.Val_.DevName_; } set { this.Val_.DevName_ = value; } }
      public string DevParams_ { get { return this.Val_.DevParams_; } set { this.Val_.DevParams_ = value; } }
      public RcFlag RcFlags_ { get { return this.Val_.RcFlags_; } set { this.Val_.RcFlags_ = value; } }
      public ClientLogFlag LogFlags_ { get { return this.Val_.LogFlags_; }  set { this.Val_.LogFlags_ = value; } }
      public FnOnLinkEv FnOnLinkEv_ { get { return this.Val_.FnOnLinkEv_; } set { this.Val_.FnOnLinkEv_ = value; } }
      // UserData_ 保留給 ClientSession 使用.
      // public IntPtr UserData_ { get { return this.Val_.UserData_; } set { this.Val_.UserData_ = value; } }

      public void Dispose()
      {
         uint idx = 0;
         foreach (IntPtr p in this.Val_.FunctionNoteParams_)
         {
            if (p != IntPtr.Zero)
            {
               this.ParamsDestructor_[idx](p);
               Marshal.FreeHGlobal(p);
            }
            ++idx;
         }
      }

      /// 設定 rc 功能參數.
      /// 如果您要啟用 rc 的某項功能, 除了 Server 端必須提供該功能之外,
      /// 您必須針對所需的功能, 提供該功能所需的參數.
      public void SetFuncParams<T>(T param) where T : FunctionNoteParams
      {
         int idx = (int)param.FunctionCode_;
         bool fDeleteOld = (this.Val_.FunctionNoteParams_[idx] != IntPtr.Zero);
         if (!fDeleteOld)
            this.Val_.FunctionNoteParams_[idx] = Marshal.AllocHGlobal(Marshal.SizeOf(param));
         Marshal.StructureToPtr(param, this.Val_.FunctionNoteParams_[idx], fDeleteOld);
         this.ParamsDestructor_[idx] = Marshal.DestroyStructure<T>;
      }
   }
}
