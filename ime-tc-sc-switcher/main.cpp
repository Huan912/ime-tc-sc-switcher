#include <windows.h>
#include <string>

namespace reg
{
    const wchar_t* kSubKey = L"SOFTWARE\\Microsoft\\IME\\15.0\\IMETC";
    const wchar_t* kValueName = L"Enable Simplified Chinese Output";
    const wchar_t* kTraditional = L"0x00000000";
    const wchar_t* kSimplified = L"0x00000001";

    bool Read(std::wstring& out)
    {
        wchar_t buf[64] = {};
        DWORD cb = sizeof(buf);
        DWORD type = 0;
        LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, kSubKey, kValueName, RRF_RT_REG_SZ, &type, buf, &cb);
        if (rc != ERROR_SUCCESS)
            return false;
        out.assign(buf);
        return true;
    }

    bool Write(const wchar_t* value)
    {
        DWORD cb = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
        LSTATUS rc = RegSetKeyValueW(HKEY_CURRENT_USER, kSubKey, kValueName, REG_SZ, value, cb);
        return rc == ERROR_SUCCESS;
    }
}

// 螢幕右下角的淡出通知視窗。
namespace osd
{
    enum { kTimerHold = 1, kTimerFade = 2 };

    const int kHoldMs = 500;
    const int kFadeStepMs = 20;
    const int kFadeStepAlpha = 8;

    bool g_enabled = true;
    HWND g_hwnd = nullptr;
    HFONT g_font = nullptr;
    std::wstring g_text;
    int g_alpha = 255;

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            HBRUSH bg = CreateSolidBrush(RGB(0x22, 0x22, 0x22));
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            HFONT old = static_cast<HFONT>(SelectObject(hdc, g_font));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0xFF, 0xFF, 0xFF));
            DrawTextW(hdc, g_text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, old);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wParam == kTimerHold)
            {
                KillTimer(hwnd, kTimerHold);
                SetTimer(hwnd, kTimerFade, kFadeStepMs, nullptr);
            }
            else if (wParam == kTimerFade)
            {
                g_alpha -= kFadeStepAlpha;
                if (g_alpha <= 0)
                {
                    KillTimer(hwnd, kTimerFade);
                    g_alpha = 255;
                    ShowWindow(hwnd, SW_HIDE);
                }
                else
                {
                    SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(g_alpha), LWA_ALPHA);
                }
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void Create(HINSTANCE hInst)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"ImeSwitcherOsd";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);

        g_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"ImeSwitcherOsd", L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, hInst, nullptr);

        // 微軟正黑體 20pt 粗體
        HDC screen = GetDC(nullptr);
        int height = -MulDiv(20, GetDeviceCaps(screen, LOGPIXELSY), 72);
        ReleaseDC(nullptr, screen);
        g_font = CreateFontW(height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft JhengHei");
    }

    void Show(const wchar_t* text)
    {
        g_text = text;

        // 量測文字大小
        HDC hdc = GetDC(g_hwnd);
        HFONT old = static_cast<HFONT>(SelectObject(hdc, g_font));
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, g_text.c_str(), static_cast<int>(g_text.size()), &sz);
        SelectObject(hdc, old);
        ReleaseDC(g_hwnd, hdc);

        const int padX = 24;
        const int padY = 14;
        int w = sz.cx + padX * 2;
        int h = sz.cy + padY * 2;

        // 對齊工作區右下角，避免被工作列遮住
        RECT work = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int margin = 20;
        int x = work.right - w - margin;
        int y = work.bottom - h - margin;

        SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);

        const int radius = 10;
        HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, radius * 2, radius * 2);
        SetWindowRgn(g_hwnd, rgn, TRUE); // 視窗接管 rgn，毋須自行刪除

        // 重置透明度並顯示
        KillTimer(g_hwnd, kTimerFade);
        KillTimer(g_hwnd, kTimerHold);
        g_alpha = 255;
        SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
        ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
        InvalidateRect(g_hwnd, nullptr, TRUE);
        UpdateWindow(g_hwnd);

        SetTimer(g_hwnd, kTimerHold, kHoldMs, nullptr);
    }
}

// 輸入法切換與重載。
namespace ime
{
    void ForceReload(HWND prevWindow)
    {
        if (!prevWindow)
            return;

        DWORD prevThread = GetWindowThreadProcessId(prevWindow, nullptr);
        HKL current = GetKeyboardLayout(prevThread);

        // 找出一個與目前不同的鍵盤配置
        HKL list[16] = {};
        int count = GetKeyboardLayoutList(16, list);
        HKL other = nullptr;
        for (int i = 0; i < count; ++i)
        {
            if (list[i] != current)
            {
                other = list[i];
                break;
            }
        }

        if (other)
        {
            PostMessageW(prevWindow, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(other));
            Sleep(80);
            PostMessageW(prevWindow, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(current));
            return;
        }

        // 只有單一鍵盤配置時，用焦點切換逼輸入法重載
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!tray)
            return;
        DWORD curThread = GetCurrentThreadId();
        BOOL attached = (prevThread && prevThread != curThread) ? AttachThreadInput(curThread, prevThread, TRUE) : FALSE;
        SetForegroundWindow(tray);
        Sleep(50);
        SetForegroundWindow(prevWindow);
        SetFocus(prevWindow);
        if (attached)
            AttachThreadInput(curThread, prevThread, FALSE);
    }

    void Toggle()
    {
        HWND prevWindow = GetForegroundWindow();

        std::wstring cur;
        if (!reg::Read(cur))
        {
            MessageBoxW(nullptr, L"錯誤：輸入法註冊表項目不存在\n請確認已安裝微軟注音輸入法。", L"錯誤", MB_ICONWARNING | MB_OK);
            return;
        }

        const wchar_t* next = nullptr;
        const wchar_t* label = nullptr;
        if (cur == reg::kSimplified)
        {
            next = reg::kTraditional;
            label = L"繁體";
        }
        else if (cur == reg::kTraditional)
        {
            next = reg::kSimplified;
            label = L"簡體";
        }
        else
        {
            next = reg::kTraditional; // 數值不在預期範圍，預設切回繁體
            label = L"繁體";
        }

        if (!reg::Write(next))
        {
            MessageBoxW(nullptr, L"錯誤：無法寫入註冊表。", L"錯誤", MB_ICONWARNING | MB_OK);
            return;
        }

        ForceReload(prevWindow);

        if (osd::g_enabled)
            osd::Show(label);
    }
}

enum { kHotkeyToggle = 1, kHotkeyNotify = 2 };

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"ime-tc-sc-switcher.singleton");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr, L"程式已在執行中。", L"ime-tc-sc-switcher", MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    osd::Create(hInstance);

    BOOL ok1 = RegisterHotKey(nullptr, kHotkeyToggle, MOD_SHIFT | MOD_ALT, 'F');
    BOOL ok2 = RegisterHotKey(nullptr, kHotkeyNotify, MOD_CONTROL | MOD_SHIFT | MOD_ALT, 'X');
    if (!ok1 || !ok2)
    {
        MessageBoxW(nullptr, L"錯誤：無法註冊全域快捷鍵\n可能與其他程式衝突。", L"錯誤", MB_ICONWARNING | MB_OK);
    }

    MessageBoxW(nullptr, L"使用說明:\n切換繁簡: Shift+Alt+F\n開關通知: Ctrl+Shift+Alt+X\n注意!若切換後無效果，請手動切換其他輸入法再切回中文輸入法", L"ime-tc-sc-switcher", MB_ICONINFORMATION | MB_OK);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (msg.message == WM_HOTKEY)
        {
            if (msg.wParam == kHotkeyToggle)
            {
                ime::Toggle();
            }
            else if (msg.wParam == kHotkeyNotify)
            {
                osd::g_enabled = !osd::g_enabled;
                osd::Show(osd::g_enabled ? L"通知已開啟" : L"通知已關閉");
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(nullptr, kHotkeyToggle);
    UnregisterHotKey(nullptr, kHotkeyNotify);
    if (osd::g_font)
        DeleteObject(osd::g_font);
    if (mutex)
        CloseHandle(mutex);
    return 0;
}
