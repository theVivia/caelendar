using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

public class TrayComboTest
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct NID
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
    public static extern bool Shell_NotifyIcon(uint msg, ref NID data);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr CreateWindowExW(uint ex, string cls, string title, uint style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr DefWindowProcW(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetMessageW(out MSG m, IntPtr h, uint min, uint max);
    [DllImport("user32.dll")] public static extern bool TranslateMessage(ref MSG m);
    [DllImport("user32.dll")] public static extern IntPtr DispatchMessageW(ref MSG m);
    [DllImport("user32.dll")] public static extern ushort RegisterClassW(ref WNDCLASS wc);
    [DllImport("user32.dll")] public static extern void PostQuitMessage(int code);
    [DllImport("kernel32.dll")] public static extern IntPtr GetModuleHandle(string name);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct WNDCLASS
    {
        public uint style;
        public IntPtr lpfnWndProc;
        public int cbClsExtra;
        public int cbWndExtra;
        public IntPtr hInstance;
        public IntPtr hIcon;
        public IntPtr hCursor;
        public IntPtr hbrBackground;
        public string lpszMenuName;
        public string lpszClassName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MSG
    {
        public IntPtr hwnd;
        public uint message;
        public IntPtr wParam;
        public IntPtr lParam;
        public uint time;
        public int x;
        public int y;
    }

    static Guid g = new Guid("2F1B7E4C-8A3D-4C5E-9B1F-7A6D2E8C4B90");
    static IntPtr g_hwndHidden = IntPtr.Zero;

    [STAThread]
    public static void Main()
    {
        string outPath = @"E:\myCalendar\logs\tray-combo-result.txt";
        using (var w = new StreamWriter(outPath, false))
        {
            try
            {
                // 创建专用隐藏窗口 + 消息循环线程
                IntPtr h = CreateWindowExW(0, "STATIC", "trayhidden", 0, 0, 0, 0, 0, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
                g_hwndHidden = h;
                w.WriteLine("hiddenHwnd=" + h);

                var form = new Form();
                form.Text = "combo";
                form.ShowInTaskbar = false;
                form.WindowState = FormWindowState.Minimized;
                form.Show();
                Application.DoEvents();
                IntPtr mainHwnd = form.Handle;

                w.WriteLine("=== 组合测试 ===");
                // 1) 主窗口 + 标准
                w.WriteLine("A main+std: " + Try(mainHwnd, 1, false, false));
                // 2) 隐藏窗口 + 标准
                w.WriteLine("B hidden+std: " + Try(h, 2, false, false));
                // 3) 主窗口 + SETVERSION先行
                w.WriteLine("C main+setver: " + Try(mainHwnd, 3, true, false));
                // 4) 隐藏窗口 + SETVERSION
                w.WriteLine("D hidden+setver: " + Try(h, 4, true, false));
                // 5) 主窗口 + GUID
                w.WriteLine("E main+guid: " + Try(mainHwnd, 5, false, true));
                // 6) 隐藏窗口 + GUID
                w.WriteLine("F hidden+guid: " + Try(h, 6, false, true));
                // 7) GUID + SETVERSION
                w.WriteLine("G hidden+guid+setver: " + Try(h, 7, true, true));

                // 清理成功添加的
                for (uint i = 1; i <= 7; i++) { var d = new NID(); d.uID = i; d.cbSize = 976; d.hWnd = h; Shell_NotifyIcon(2, ref d); }
                form.Close();
                if (h != IntPtr.Zero) DestroyWindow(h);
                w.WriteLine("=== 完成 ===");
            }
            catch (Exception ex)
            {
                w.WriteLine("EX: " + ex.GetType().Name + ": " + ex.Message);
            }
        }
    }

    static string Try(IntPtr hwnd, uint uid, bool setVersionFirst, bool useGuid)
    {
        try
        {
            var nid = new NID();
            nid.cbSize = 976;
            nid.hWnd = hwnd;
            nid.uID = uid;
            nid.uFlags = 0x1 | 0x2 | 0x4; // ICON | MESSAGE | TIP
            nid.uCallbackMessage = 0x0400u + uid;
            nid.hIcon = System.Drawing.SystemIcons.Application.Handle;
            nid.szTip = "COMBO" + uid;
            if (useGuid)
            {
                nid.uFlags |= 0x20; // NIF_GUID
                nid.guidItem = g;
            }
            if (setVersionFirst)
            {
                var ver = nid;
                ver.uFlags = 0;
                ver.uVersion = 4; // NOTIFYICON_VERSION_4
                // 先 ADD（不带版本）再 SETVERSION 是标准；这里尝试 ADD 前 SETVERSION（非标）
                Shell_NotifyIcon(1, ref ver);
                Shell_NotifyIcon(4, ref ver); // NIM_SETVERSION
            }
            Shell_NotifyIcon(0, ref nid); // NIM_MODIFY to reset last error? 不用
            bool ok = Shell_NotifyIcon(1, ref nid);
            int err = Marshal.GetLastWin32Error();
            if (ok && setVersionFirst) { var v2 = nid; Shell_NotifyIcon(4, ref v2); }
            return "ok=" + ok + " err=" + err;
        }
        catch (Exception ex) { return "EX " + ex.Message; }
    }
}
