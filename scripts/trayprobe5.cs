using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

public class TrayProbe5 {
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

    [STAThread]
    public static void Main() {
        string outPath = @"E:\myCalendar\logs\trayprobe5.txt";
        using (var w = new StreamWriter(outPath, false)) {
            try {
                var form = new Form();
                form.Text = "probe5";
                form.ShowInTaskbar = false;
                form.WindowState = FormWindowState.Minimized;
                form.Show();
                Application.DoEvents();
                IntPtr h = form.Handle;
                w.WriteLine("hwnd=" + h);
                var nid = new NID();
                nid.cbSize = 976;
                nid.hWnd = h;
                nid.uID = 7;
                nid.uFlags = 0x1 | 0x2 | 0x4;
                nid.uCallbackMessage = 0x0400 + 200;
                nid.hIcon = System.Drawing.SystemIcons.Application.Handle;
                nid.szTip = "PROBE5";
                bool ok = Shell_NotifyIcon(1, ref nid);
                int err = Marshal.GetLastWin32Error();
                w.WriteLine("NIM_ADD ok=" + ok + " err=" + err + " hIcon=" + nid.hIcon + " cbSize=" + nid.cbSize);
                if (ok) { var del = nid; Shell_NotifyIcon(2, ref del); w.WriteLine("NIM_DELETE done"); }
                form.Close();
            } catch (Exception ex) { w.WriteLine("EX: " + ex.GetType().Name + ": " + ex.Message); }
        }
    }
}