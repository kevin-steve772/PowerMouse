#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>

// ===================== Windows =====================
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

static constexpr UINT WM_APP_TRAY   = WM_APP + 1;
static constexpr UINT WM_APP_EXIT   = WM_APP + 2;
static constexpr UINT ID_TRAY       = 1;
static constexpr UINT ID_HOTKEY_F6  = 100;
static constexpr UINT ID_TIMER_CLICK = 200;

static HWND g_hwnd        = nullptr;
static HWND g_hwndOptions = nullptr;
static HICON g_hIconApp   = nullptr;
static bool g_running = false;
static int  g_cps     = 8;
static bool g_left    = true;
static bool g_right   = true;
static int  g_interval_ms = 125;

static void UpdateInterval() {
    g_interval_ms = 1000 / g_cps;
}

static void ShowBalloon(const wchar_t *title, const wchar_t *text) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_hwnd;
    nid.uID    = ID_TRAY;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_NONE;
    wcscpy_s(nid.szInfoTitle, title);
    wcscpy_s(nid.szInfo, text);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static HICON CreateAppIcon() {
    const int S = 32;
    uint32_t buf[S * S] = {};
    int r2 = (S/2 - 3) * (S/2 - 3);
    uint32_t blue = 0xFF0078D7;

    auto setpx = [&](int x, int y, uint32_t c) {
        if (x >= 0 && x < S && y >= 0 && y < S)
            buf[y * S + x] = c;
    };
    auto fill = [&](int x1, int y1, int x2, int y2, uint32_t c) {
        for (int y = y1; y <= y2; y++)
            for (int x = x1; x <= x2; x++)
                setpx(x, y, c);
    };

    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            int dx = x - S/2, dy = y - S/2;
            if (dx*dx + dy*dy <= r2)
                buf[y * S + x] = blue;
        }

    uint32_t white = 0xFFFFFFFF;
    fill(11,  6, 15, 25, white);
    fill(11,  6, 24, 10, white);
    fill(21,  6, 24, 16, white);
    fill(11, 15, 24, 19, white);

    BITMAPV5HEADER bi{};
    bi.bV5Size       = sizeof(bi);
    bi.bV5Width      = S;
    bi.bV5Height     = -S;
    bi.bV5Planes     = 1;
    bi.bV5BitCount   = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask    = 0x00FF0000;
    bi.bV5GreenMask  = 0x0000FF00;
    bi.bV5BlueMask   = 0x000000FF;
    bi.bV5AlphaMask  = 0xFF000000;

    HDC hdc = GetDC(nullptr);
    void *bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hBmp && bits) memcpy(bits, buf, sizeof(buf));
    HBITMAP hMask = CreateBitmap(S, S, 1, 1, nullptr);
    ICONINFO ii = {TRUE, 0, 0, hMask, hBmp};
    HICON hIcon = CreateIconIndirect(&ii);
    if (hMask) DeleteObject(hMask);
    if (hBmp) DeleteObject(hBmp);
    ReleaseDC(nullptr, hdc);
    return hIcon;
}

static void AddTrayIcon() {
    g_hIconApp = CreateAppIcon();
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_hwnd;
    nid.uID    = ID_TRAY;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon  = g_hIconApp;
    wcscpy_s(nid.szTip, L"PowerMouse - 鼠标连点器");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void RemoveTrayIcon() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_hwnd;
    nid.uID    = ID_TRAY;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void StartClicking() {
    if (g_running) return;
    g_running = true;
    SetTimer(g_hwnd, ID_TIMER_CLICK, g_interval_ms, nullptr);
    std::wstring mode;
    if (g_left)  mode += L"左键 ";
    if (g_right) mode += L"右键 ";
    wchar_t buf[128];
    swprintf(buf, 128, L"已启动  %s| %d CPS", mode.c_str(), g_cps);
    ShowBalloon(L"PowerMouse", buf);
}

static void StopClicking() {
    if (!g_running) return;
    g_running = false;
    KillTimer(g_hwnd, ID_TIMER_CLICK);
    ShowBalloon(L"PowerMouse", L"已停止");
}

static void ToggleClicking() {
    if (g_running) StopClicking();
    else           StartClicking();
}

static void DoClick() {
    INPUT inputs[4] = {};
    int count = 0;
    if (g_left) {
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        count++;
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        count++;
    }
    if (g_right) {
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        count++;
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        count++;
    }
    if (count > 0)
        SendInput(count, inputs, sizeof(INPUT));
}

static constexpr int IDC_OPT_LEFT  = 4001;
static constexpr int IDC_OPT_RIGHT = 4002;
static constexpr int IDC_OPT_SLIDER = 4003;
static constexpr int IDC_OPT_CPS   = 4004;
static constexpr int IDC_OPT_OK    = 4005;

static LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowExW(0, L"BUTTON", L"左键",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 20, 130, 26, hwnd, (HMENU)IDC_OPT_LEFT, nullptr, nullptr);
        SendMessage(GetDlgItem(hwnd, IDC_OPT_LEFT), BM_SETCHECK,
            g_left ? BST_CHECKED : BST_UNCHECKED, 0);

        CreateWindowExW(0, L"BUTTON", L"右键",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 55, 130, 26, hwnd, (HMENU)IDC_OPT_RIGHT, nullptr, nullptr);
        SendMessage(GetDlgItem(hwnd, IDC_OPT_RIGHT), BM_SETCHECK,
            g_right ? BST_CHECKED : BST_UNCHECKED, 0);

        CreateWindowExW(0, L"STATIC", L"CPS:",
            WS_CHILD | WS_VISIBLE,
            20, 98, 35, 20, hwnd, nullptr, nullptr, nullptr);

        HWND hSld = CreateWindowExW(0, L"msctls_trackbar32", nullptr,
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            60, 95, 155, 26, hwnd, (HMENU)IDC_OPT_SLIDER, nullptr, nullptr);
        SendMessage(hSld, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
        SendMessage(hSld, TBM_SETPOS, TRUE, g_cps);

        wchar_t tmp[16];
        swprintf(tmp, 16, L"%d", g_cps);
        CreateWindowExW(0, L"STATIC", tmp,
            WS_CHILD | WS_VISIBLE,
            220, 98, 30, 20, hwnd, (HMENU)IDC_OPT_CPS, nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            100, 145, 80, 28, hwnd, (HMENU)IDC_OPT_OK, nullptr, nullptr);
        return 0;
    }

    case WM_HSCROLL:
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_OPT_SLIDER)) {
            int val = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            wchar_t tmp[16];
            swprintf(tmp, 16, L"%d", val);
            SetWindowTextW(GetDlgItem(hwnd, IDC_OPT_CPS), tmp);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_OPT_OK) {
            g_left  = SendMessage(GetDlgItem(hwnd, IDC_OPT_LEFT), BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_right = SendMessage(GetDlgItem(hwnd, IDC_OPT_RIGHT), BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_cps   = (int)SendMessage(GetDlgItem(hwnd, IDC_OPT_SLIDER), TBM_GETPOS, 0, 0);
            if (!g_left && !g_right) g_left = true;
            UpdateInterval();
            if (g_running) {
                StopClicking();
                StartClicking();
            }
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_hwndOptions = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ShowOptionsDialog(HWND hwndParent) {
    if (g_hwndOptions) {
        SetForegroundWindow(g_hwndOptions);
        return;
    }

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwndParent, GWLP_HINSTANCE);

    WNDCLASSW wc{};
    wc.lpfnWndProc   = OptionsWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PowerMouseOptions";
    RegisterClassW(&wc);

    g_hwndOptions = CreateWindowExW(0, L"PowerMouseOptions", L"PowerMouse 选项",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 225,
        hwndParent, nullptr, hInst, nullptr);

    if (g_hwndOptions) {
        RECT rc;
        GetWindowRect(g_hwndOptions, &rc);
        int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
        SetWindowPos(g_hwndOptions, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        ShowWindow(g_hwndOptions, SW_SHOW);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_APP_TRAY:
        if (lParam == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, g_running ? L"停止" : L"启动");
            AppendMenuW(menu, MF_STRING, 2, L"选项");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 3, L"退出");
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            if (cmd == 1) ToggleClicking();
            else if (cmd == 2) ShowOptionsDialog(hwnd);
            else if (cmd == 3) PostMessage(hwnd, WM_CLOSE, 0, 0);
        } else if (lParam == WM_LBUTTONUP) {
            ToggleClicking();
        }
        return 0;

    case WM_HOTKEY:
        if (wParam == ID_HOTKEY_F6)
            ToggleClicking();
        return 0;

    case WM_TIMER:
        if (wParam == ID_TIMER_CLICK)
            DoClick();
        return 0;

    case WM_CLOSE:
        if (g_running) StopClicking();
        UnregisterHotKey(hwnd, ID_HOTKEY_F6);
        RemoveTrayIcon();
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_hIconApp) { DestroyIcon(g_hIconApp); g_hIconApp = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void RunPlatform(const char *) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSW wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PowerMouseClass";
    if (!RegisterClassW(&wc)) return;

    g_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"PowerMouseClass",
        L"PowerMouse",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        300, 200,
        nullptr, nullptr, hInst, nullptr);

    if (!g_hwnd) {
        UnregisterClassW(L"PowerMouseClass", hInst);
        return;
    }

    AddTrayIcon();
    if (!RegisterHotKey(g_hwnd, ID_HOTKEY_F6, 0, VK_F6))
        ShowBalloon(L"PowerMouse", L"热键 F6 注册失败");
    else
        ShowBalloon(L"PowerMouse", L"已启动  按 F6 开始/停止");

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassW(L"PowerMouseClass", hInst);
}

#elif defined(__linux__)
// ===================== Linux (X11) =====================
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

static Display *g_disp = nullptr;
static Atom     g_wm_delete_window;
static Window   g_win = 0;
static bool     g_running = false;
static bool     g_quit = false;
static int      g_cps = 8;
static bool     g_left = true;
static bool     g_right = true;
static int      g_interval_ms = 125;
static timespec g_last_click = {};

static void UpdateInterval() {
    g_interval_ms = 1000 / g_cps;
}

static void Notify(const char *summary, const char *body) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "notify-send '%s' '%s'", summary, body);
    (void)system(cmd);
}

static void DoClick() {
    if (!g_disp) return;
    if (g_left) {
        XTestFakeButtonEvent(g_disp, Button1, True, 0);
        XTestFakeButtonEvent(g_disp, Button1, False, 0);
    }
    if (g_right) {
        XTestFakeButtonEvent(g_disp, Button3, True, 0);
        XTestFakeButtonEvent(g_disp, Button3, False, 0);
    }
    XFlush(g_disp);
}

static void StartClicking() {
    if (g_running) return;
    g_running = true;
    clock_gettime(CLOCK_MONOTONIC, &g_last_click);
    std::string mode;
    if (g_left)  mode += "左键 ";
    if (g_right) mode += "右键 ";
    char buf[128];
    snprintf(buf, sizeof(buf), "已启动  %s| %d CPS", mode.c_str(), g_cps);
    Notify("PowerMouse", buf);
}

static void StopClicking() {
    if (!g_running) return;
    g_running = false;
    Notify("PowerMouse", "已停止");
}

static void ToggleClicking() {
    if (g_running) StopClicking();
    else           StartClicking();
}

static void GrabF6(Display *d, Window root) {
    KeyCode kc = XKeysymToKeycode(d, XK_F6);
    XGrabKey(d, kc, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XSync(d, False);
}

static void RunPlatform(const char *mode_str) {
    g_disp = XOpenDisplay(nullptr);
    if (!g_disp) {
        fprintf(stderr, "无法打开 X11 显示，请检查 DISPLAY 环境变量\n");
        exit(1);
    }

    int ev, err;
    if (!XTestQueryExtension(g_disp, &ev, &err)) {
        fprintf(stderr, "XTest 扩展不可用\n");
        XCloseDisplay(g_disp);
        exit(1);
    }

    int screen = DefaultScreen(g_disp);
    Window root = RootWindow(g_disp, screen);

    g_win = XCreateSimpleWindow(g_disp, root, 0, 0, 1, 1, 0, 0, 0);
    g_wm_delete_window = XInternAtom(g_disp, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_disp, g_win, &g_wm_delete_window, 1);
    XSelectInput(g_disp, g_win, KeyPressMask | StructureNotifyMask);

    GrabF6(g_disp, root);
    Notify("PowerMouse", "已启动  按 F6 开始/停止");

    clock_gettime(CLOCK_MONOTONIC, &g_last_click);

    XEvent evt;
    while (!g_quit) {
        while (XPending(g_disp)) {
            XNextEvent(g_disp, &evt);
            if (evt.type == KeyPress) {
                KeySym ks = XLookupKeysym(&evt.xkey, 0);
                if (ks == XK_F6)
                    ToggleClicking();
            }
            if (evt.type == ClientMessage &&
                (Atom)evt.xclient.data.l[0] == g_wm_delete_window) {
                g_quit = true;
            }
        }

        if (g_running) {
            timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed_ms = (now.tv_sec - g_last_click.tv_sec) * 1000 +
                              (now.tv_nsec - g_last_click.tv_nsec) / 1000000;
            if (elapsed_ms >= g_interval_ms) {
                DoClick();
                g_last_click = now;
            }
        }
        usleep(5000);
    }

    if (g_running) StopClicking();
    XDestroyWindow(g_disp, g_win);
    XCloseDisplay(g_disp);
}

#else
#error "不支持此平台"
#endif

// ===================== 通用入口 =====================
static void PrintUsage(const char *name) {
    fprintf(stderr,
        "PowerMouse - 鼠标连点器 (Minecraft 生电挂机)\n"
        "用法: %s [选项]\n\n"
        "选项:\n"
        "  --cps N      每秒点击次数 (1-100, 默认 8)\n"
        "  --left       仅左键点击 (默认)\n"
        "  --right      仅右键点击\n"
        "  --both       左右键同时点击\n"
        "  --help       显示此帮助\n\n"
        "热键: F6 启动/停止\n",
        name);
}

int main(int argc, char *argv[]) {
    g_cps   = 8;
    g_left  = true;
    g_right = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cps") == 0 && i + 1 < argc) {
            int val = atoi(argv[++i]);
            if (val < 1) val = 1;
            if (val > 100) val = 100;
            g_cps = val;
        } else if (strcmp(argv[i], "--left") == 0) {
            g_left  = true;
            g_right = false;
        } else if (strcmp(argv[i], "--right") == 0) {
            g_left  = false;
            g_right = true;
        } else if (strcmp(argv[i], "--both") == 0) {
            g_left  = true;
            g_right = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "未知选项: %s\n", argv[0]);
            PrintUsage(argv[0]);
            return 1;
        }
    }

    UpdateInterval();
    RunPlatform(argv[0]);
    return 0;
}
