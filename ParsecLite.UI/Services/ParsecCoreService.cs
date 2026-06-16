using System;
using System.Runtime.InteropServices;

namespace ParsecLite.UI.Services
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct ParsecConfig
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string SelectedIp;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string HostIp;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string Username;
        public int Bitrate;
        public int Fps;
        [MarshalAs(UnmanagedType.I1)]
        public bool IsHost;
        [MarshalAs(UnmanagedType.I1)]
        public bool UseHardwareEncoding;
        public IntPtr WindowHandle;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ParsecTelemetry
    {
        public float E2eLatency;
        public float CaptureTime;
        public float EncodeTime;
        public float NetworkTime;
        public float DecodeTime;
        public float Fps;
        public float LossRate;
        public float Rtt;
        public float BitrateMbps;
    }

    public class ParsecCoreService
    {
        private const string DllName = "ParsecLiteCore.dll";

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool Parsec_Initialize();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Parsec_StartSession(ParsecConfig config);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Parsec_StopSession();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool Parsec_GetTelemetry(out ParsecTelemetry telemetry);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Parsec_Shutdown();
    }
}
