using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

public class TrayTest
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct NOTIFYICONDATA
    {
        public int cbSize;
        public IntPtr hWnd;
        public uint uID;
        public uint uFlags;
        public uint uCallbackMessage;
        public IntPtr hIcon;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string szTip;
        public uint dwState;
        public uint dwStateMask;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string szInfo;
        public uint uVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string szInfoTitle;
        public uint dwInfoFlags;
        public Guid guidItem;
        public IntPtr hBalloonIcon;
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool Shell_NotifyIcon(uint msg, ref NOTIFYICONDATA data);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetLastErrorEx(uint err, uint type);
    [DllImport("kernel32.dll")]
    public static extern void SetLastError(uint err);

    [STAThread]
    public static void Main()
    {
        string outPath = @"E:\myCalendar\logs\traytest-result.txt";
        using (var w = new StreamWriter(outPath, false))
        {
            try
            {
                var form = new Form();
                form.Text = "TrayTest";
                form.ShowInTaskbar = false;
                form.WindowState = FormWindowState.Minimized;
                form.Show();
                Application.DoEvents();

                var nid = new NOTIFYICONDATA();
                nid.cbSize = Marshal.SizeOf(typeof(NOTIFYICONDATA));
                nid.hWnd = form.Handle;
                nid.uID = 4242;
                nid.uFlags = 0x1 | 0x2 | 0x4; // NIF_ICON | NIF_MESSAGE | NIF_TIP
                nid.uCallbackMessage = 0x0400 + 100;
                nid.hIcon = System.Drawing.SystemIcons.Application.Handle;
                nid.szTip = "TRAYTEST-DSH";
                SetLastError(0);
                bool ok = Shell_NotifyIcon(1, ref nid); // NIM_ADD
                int err = Marshal.GetLastWin32Error();
                w.WriteLine("NIM_ADD ok=" + ok + " err=" + err);
                if (ok)
                {
                    System.Threading.Thread.Sleep(1500);
                    var del = nid;
                    SetLastError(0);
                    bool ok2 = Shell_NotifyIcon(2, ref del); // NIM_DELETE
                    w.WriteLine("NIM_DELETE ok=" + ok2 + " err=" + Marshal.GetLastWin32Error());
                }
                form.Close();
            }
            catch (Exception ex)
            {
                w.WriteLine("EXCEPTION: " + ex.GetType().Name + ": " + ex.Message);
            }
        }
    }
}
