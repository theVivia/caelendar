using System;
using System.IO;
using System.Runtime.InteropServices;

public class TrayProbe4 {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    public struct NID {
        public int cbSize;
        public IntPtr hWnd;
        public uint uID;
        public uint uFlags;
        public uint uCallbackMessage;
        public IntPtr hIcon;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=128)] public string szTip;
        public uint dwState;
        public uint dwStateMask;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string szInfo;
        public uint uVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)] public string szInfoTitle;
        public uint dwInfoFlags;
        public Guid guidItem;
        public IntPtr hBalloonIcon;
    }
    [DllImport("shell32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool Shell_NotifyIcon(uint msg, ref NID data);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr CreateWindowExW(uint ex, string cls, string title, uint style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr LoadIconW(IntPtr inst, IntPtr name);
    [DllImport("kernel32.dll")] public static extern void SetLastError(uint err);
    [DllImport("user32.dll")] public static extern IntPtr GetDesktopWindow();
    [DllImport("user32.dll")] public static extern IntPtr GetShellWindow();

    public static void Run() {
        string outPath = @"E:\myCalendar\logs\trayprobe4.txt";
        using (var w = new StreamWriter(outPath, false))
        {
            try {
                w.WriteLine("desktop=" + GetDesktopWindow() + " shell=" + GetShellWindow());
                // 用真实顶层窗口（STATIC 子窗口不行，需有自己的消息循环窗口）
                IntPtr h = CreateWindowExW(0, "STATIC", "probe4", 0x10000000 /*WS_VISIBLE*/, 0, 0, 100, 100, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
                w.WriteLine("hwnd=" + h);
                if (h == IntPtr.Zero) { w.WriteLine("CreateWindow FAILED err=" + Marshal.GetLastWin32Error()); return; }
                var nid = new NID();
                nid.cbSize = 976;  // 与星历一致
                nid.hWnd = h;
                nid.uID = 7;
                nid.uFlags = 0x1 | 0x2 | 0x4;
                nid.uCallbackMessage = 0x0400 + 200;
                nid.hIcon = LoadIconW(IntPtr.Zero, (IntPtr)32512);
                nid.szTip = "PROBE4";
                SetLastError(0);
                bool ok = Shell_NotifyIcon(1, ref nid);
                int err = Marshal.GetLastWin32Error();
                w.WriteLine("NIM_ADD ok=" + ok + " err=" + err + " hIcon=" + nid.hIcon + " cbSize=" + nid.cbSize);
                if (ok) { var del = nid; Shell_NotifyIcon(2, ref del); w.WriteLine("NIM_DELETE done"); }
                DestroyWindow(h);
            } catch (Exception ex) { w.WriteLine("EX: " + ex.Message); }
        }
    }
}