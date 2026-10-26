using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

public class TrayApp {
    [DllImport("shell32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool Shell_NotifyIcon(uint msg, ref NOTIFYICONDATA data);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    public struct NOTIFYICONDATA {
        public int cbSize; public IntPtr hWnd; public uint uID; public uint uFlags;
        public uint uCallbackMessage; public IntPtr hIcon;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=128)] public string szTip;
        public uint dwState; public uint dwStateMask;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string szInfo;
        public uint uVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)] public string szInfoTitle;
        public uint dwInfoFlags; public Guid guidItem; public IntPtr hBalloonIcon;
    }
    [STAThread]
    public static void Main() {
        string outPath = @"E:\myCalendar\logs\trayapp-result.txt";
        var form = new Form();
        form.Text = "TrayAppTest";
        form.ShowInTaskbar = false;
        form.WindowState = FormWindowState.Minimized;
        form.Load += (s, e) => {
            using (var w = new StreamWriter(outPath, false)) {
                var nid = new NOTIFYICONDATA();
                nid.cbSize = 976;
                nid.hWnd = form.Handle;
                nid.uID = 1;
                nid.uFlags = 0x1 | 0x2 | 0x4;
                nid.uCallbackMessage = 0x0400 + 300;
                nid.hIcon = System.Drawing.SystemIcons.Application.Handle;
                nid.szTip = "TRAYAPP";
                bool ok = Shell_NotifyIcon(1, ref nid);
                int err = Marshal.GetLastWin32Error();
                w.WriteLine("NIM_ADD ok=" + ok + " err=" + err);
                var t = new Timer(); t.Interval = 1500;
                t.Tick += (s2, e2) => { t.Stop(); if (ok) { var d = nid; Shell_NotifyIcon(2, ref d); } form.Close(); };
                t.Start();
            }
        };
        Application.Run(form);
    }
}