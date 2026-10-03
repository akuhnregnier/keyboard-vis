/*
 * KeyViz - live keyboard visualiser for Windows
 *
 * Shows a full-size keyboard and highlights every key that is currently held
 * down, in real time. Uses a global low-level keyboard hook (WH_KEYBOARD_LL),
 * so it keeps working while another window has focus.
 *
 * Keys are identified by hardware scan code (physical position), so the layout
 * is correct regardless of the active keyboard language. Character labels on
 * letter/number/punctuation keys come from the active layout at startup.
 *
 * Build (MinGW):  gcc keyviz.c -o KeyViz.exe -O2 -mwindows -lgdi32 -luser32
 * Build (MSVC):   cl /O2 keyviz.c user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
 *
 * Extras:
 *   - Released keys fade out over a short period, so quick taps are visible.
 *   - Bottom bar lists the names of all currently held keys (including keys not
 *     drawn on the board, e.g. media keys).
 *   - "Always on top" toggle in the window's system menu (right-click title bar).
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

#define FADE_MS        220
#define TIMER_ID       1
#define TIMER_MS       16
#define IDM_TOPMOST    0x0010
#define BOARD_W        22.5f
#define BOARD_H        6.5f

/* Colours */
#define COL_BG         RGB(0x1b, 0x1c, 0x21)
#define COL_KEY        RGB(0x2b, 0x2d, 0x35)
#define COL_KEY_EDGE   RGB(0x3c, 0x3f, 0x4a)
#define COL_TEXT       RGB(0xb8, 0xbc, 0xc6)
#define COL_HOT        RGB(0x3d, 0x8b, 0xff)
#define COL_HOT_EDGE   RGB(0x8f, 0xbf, 0xff)
#define COL_HOT_TEXT   RGB(0xff, 0xff, 0xff)
#define COL_STATUS     RGB(0x8a, 0x8f, 0x9c)

/* Key id = scan code | 0x100 if extended. */
typedef struct {
    const wchar_t *label;   /* NULL = take character from the keyboard layout */
    float x, y, w, h;       /* in key units */
    int id;
    wchar_t dyn[8];         /* filled in at startup for NULL labels */
} Key;

#define K(l, x, y, w, h, id) { l, x, y, w, h, id, {0} }
#define ISO_ENTER 0x01C

static Key g_keys[] = {
    /* Function row */
    K(L"Esc", 0, 0, 1, 1, 0x001),
    K(L"F1", 2, 0, 1, 1, 0x03B), K(L"F2", 3, 0, 1, 1, 0x03C),
    K(L"F3", 4, 0, 1, 1, 0x03D), K(L"F4", 5, 0, 1, 1, 0x03E),
    K(L"F5", 6.5f, 0, 1, 1, 0x03F), K(L"F6", 7.5f, 0, 1, 1, 0x040),
    K(L"F7", 8.5f, 0, 1, 1, 0x041), K(L"F8", 9.5f, 0, 1, 1, 0x042),
    K(L"F9", 11, 0, 1, 1, 0x043), K(L"F10", 12, 0, 1, 1, 0x044),
    K(L"F11", 13, 0, 1, 1, 0x057), K(L"F12", 14, 0, 1, 1, 0x058),
    K(L"PrtSc", 15.25f, 0, 1, 1, 0x137),
    K(L"ScrLk", 16.25f, 0, 1, 1, 0x046),
    K(L"Pause", 17.25f, 0, 1, 1, 0x045),

    /* Number row */
    K(NULL, 0, 1.5f, 1, 1, 0x029),
    K(NULL, 1, 1.5f, 1, 1, 0x002), K(NULL, 2, 1.5f, 1, 1, 0x003),
    K(NULL, 3, 1.5f, 1, 1, 0x004), K(NULL, 4, 1.5f, 1, 1, 0x005),
    K(NULL, 5, 1.5f, 1, 1, 0x006), K(NULL, 6, 1.5f, 1, 1, 0x007),
    K(NULL, 7, 1.5f, 1, 1, 0x008), K(NULL, 8, 1.5f, 1, 1, 0x009),
    K(NULL, 9, 1.5f, 1, 1, 0x00A), K(NULL, 10, 1.5f, 1, 1, 0x00B),
    K(NULL, 11, 1.5f, 1, 1, 0x00C), K(NULL, 12, 1.5f, 1, 1, 0x00D),
    K(L"Backspace", 13, 1.5f, 2, 1, 0x00E),
    K(L"Ins", 15.25f, 1.5f, 1, 1, 0x152),
    K(L"Home", 16.25f, 1.5f, 1, 1, 0x147),
    K(L"PgUp", 17.25f, 1.5f, 1, 1, 0x149),
    K(L"Num", 18.5f, 1.5f, 1, 1, 0x145),
    K(L"/", 19.5f, 1.5f, 1, 1, 0x135),
    K(L"*", 20.5f, 1.5f, 1, 1, 0x037),
    K(L"-", 21.5f, 1.5f, 1, 1, 0x04A),

    /* Top letter row */
    K(L"Tab", 0, 2.5f, 1.5f, 1, 0x00F),
    K(NULL, 1.5f, 2.5f, 1, 1, 0x010), K(NULL, 2.5f, 2.5f, 1, 1, 0x011),
    K(NULL, 3.5f, 2.5f, 1, 1, 0x012), K(NULL, 4.5f, 2.5f, 1, 1, 0x013),
    K(NULL, 5.5f, 2.5f, 1, 1, 0x014), K(NULL, 6.5f, 2.5f, 1, 1, 0x015),
    K(NULL, 7.5f, 2.5f, 1, 1, 0x016), K(NULL, 8.5f, 2.5f, 1, 1, 0x017),
    K(NULL, 9.5f, 2.5f, 1, 1, 0x018), K(NULL, 10.5f, 2.5f, 1, 1, 0x019),
    K(NULL, 11.5f, 2.5f, 1, 1, 0x01A), K(NULL, 12.5f, 2.5f, 1, 1, 0x01B),
    K(L"Enter", 13.5f, 2.5f, 1.5f, 2, ISO_ENTER), /* drawn as ISO L-shape */
    K(L"Del", 15.25f, 2.5f, 1, 1, 0x153),
    K(L"End", 16.25f, 2.5f, 1, 1, 0x14F),
    K(L"PgDn", 17.25f, 2.5f, 1, 1, 0x151),
    K(L"7", 18.5f, 2.5f, 1, 1, 0x047), K(L"8", 19.5f, 2.5f, 1, 1, 0x048),
    K(L"9", 20.5f, 2.5f, 1, 1, 0x049),
    K(L"+", 21.5f, 2.5f, 1, 2, 0x04E),

    /* Home row */
    K(L"Caps", 0, 3.5f, 1.75f, 1, 0x03A),
    K(NULL, 1.75f, 3.5f, 1, 1, 0x01E), K(NULL, 2.75f, 3.5f, 1, 1, 0x01F),
    K(NULL, 3.75f, 3.5f, 1, 1, 0x020), K(NULL, 4.75f, 3.5f, 1, 1, 0x021),
    K(NULL, 5.75f, 3.5f, 1, 1, 0x022), K(NULL, 6.75f, 3.5f, 1, 1, 0x023),
    K(NULL, 7.75f, 3.5f, 1, 1, 0x024), K(NULL, 8.75f, 3.5f, 1, 1, 0x025),
    K(NULL, 9.75f, 3.5f, 1, 1, 0x026), K(NULL, 10.75f, 3.5f, 1, 1, 0x027),
    K(NULL, 11.75f, 3.5f, 1, 1, 0x028), K(NULL, 12.75f, 3.5f, 1, 1, 0x02B),
    K(L"4", 18.5f, 3.5f, 1, 1, 0x04B), K(L"5", 19.5f, 3.5f, 1, 1, 0x04C),
    K(L"6", 20.5f, 3.5f, 1, 1, 0x04D),

    /* Bottom letter row */
    K(L"Shift", 0, 4.5f, 1.25f, 1, 0x02A),
    K(NULL, 1.25f, 4.5f, 1, 1, 0x056),
    K(NULL, 2.25f, 4.5f, 1, 1, 0x02C), K(NULL, 3.25f, 4.5f, 1, 1, 0x02D),
    K(NULL, 4.25f, 4.5f, 1, 1, 0x02E), K(NULL, 5.25f, 4.5f, 1, 1, 0x02F),
    K(NULL, 6.25f, 4.5f, 1, 1, 0x030), K(NULL, 7.25f, 4.5f, 1, 1, 0x031),
    K(NULL, 8.25f, 4.5f, 1, 1, 0x032), K(NULL, 9.25f, 4.5f, 1, 1, 0x033),
    K(NULL, 10.25f, 4.5f, 1, 1, 0x034), K(NULL, 11.25f, 4.5f, 1, 1, 0x035),
    K(L"Shift", 12.25f, 4.5f, 2.75f, 1, 0x036),
    K(L"\x2191", 16.25f, 4.5f, 1, 1, 0x148),
    K(L"1", 18.5f, 4.5f, 1, 1, 0x04F), K(L"2", 19.5f, 4.5f, 1, 1, 0x050),
    K(L"3", 20.5f, 4.5f, 1, 1, 0x051),
    K(L"Enter", 21.5f, 4.5f, 1, 2, 0x11C),

    /* Space row */
    K(L"Ctrl", 0, 5.5f, 1.25f, 1, 0x01D),
    K(L"Win", 1.25f, 5.5f, 1.25f, 1, 0x15B),
    K(L"Alt", 2.5f, 5.5f, 1.25f, 1, 0x038),
    K(L"", 3.75f, 5.5f, 6.25f, 1, 0x039),
    K(L"AltGr", 10, 5.5f, 1.25f, 1, 0x138),
    K(L"Win", 11.25f, 5.5f, 1.25f, 1, 0x15C),
    K(L"Menu", 12.5f, 5.5f, 1.25f, 1, 0x15D),
    K(L"Ctrl", 13.75f, 5.5f, 1.25f, 1, 0x11D),
    K(L"\x2190", 15.25f, 5.5f, 1, 1, 0x14B),
    K(L"\x2193", 16.25f, 5.5f, 1, 1, 0x150),
    K(L"\x2192", 17.25f, 5.5f, 1, 1, 0x14D),
    K(L"0", 18.5f, 5.5f, 2, 1, 0x052),
    K(L".", 20.5f, 5.5f, 1, 1, 0x053),
};
#define NKEYS ((int)(sizeof(g_keys) / sizeof(g_keys[0])))

static HWND  g_hwnd;
static HHOOK g_hook;
static BYTE  g_down[512];   /* 1 while held */
static BYTE  g_vk[512];     /* last virtual-key code seen for the id */
static DWORD g_upTick[512]; /* GetTickCount() at release, for fade-out */
static BOOL  g_topmost;

/* ---------------------------------------------------------------------- */

static int key_id(const KBDLLHOOKSTRUCT *k)
{
    DWORD vk = k->vkCode, sc = k->scanCode;
    int ext = (k->flags & LLKHF_EXTENDED) != 0;

    /* A few keys report awkward scan codes; pin them down by VK. */
    switch (vk) {
    case VK_PAUSE:
    case VK_CANCEL:   return 0x045;  /* Pause / Ctrl+Break */
    case VK_NUMLOCK:  return 0x145;
    case VK_SNAPSHOT: return 0x137;  /* also Alt+PrtSc (SysRq) */
    case VK_LSHIFT:   return 0x02A;
    case VK_RSHIFT:   return 0x036;
    }
    if (sc == 0)
        sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    return (int)(((sc & 0xFF) | (ext ? 0x100 : 0)) & 0x1FF);
}

static void release(int id)
{
    g_down[id] = 0;
    g_upTick[id] = GetTickCount();
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *k = (const KBDLLHOOKSTRUCT *)lp;
        int id = key_id(k);
        BOOL down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
        if (down) {
            if (!g_down[id]) {
                g_down[id] = 1;
                g_vk[id] = (BYTE)k->vkCode;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
        } else {
            /* Release even if we never saw the press (e.g. PrtSc only
               sends key-up), so it still flashes briefly. */
            release(id);
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

/* Clear keys whose release we missed (e.g. Win+L, elevated windows). */
static void unstick_keys(void)
{
    for (int id = 0; id < 512; id++) {
        if (g_down[id] && g_vk[id] && !(GetAsyncKeyState(g_vk[id]) & 0x8000)) {
            release(id);
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
    }
}

static void load_layout_labels(void)
{
    for (int i = 0; i < NKEYS; i++) {
        Key *k = &g_keys[i];
        if (k->label) continue;
        UINT vk = MapVirtualKeyW(k->id & 0xFF, MAPVK_VSC_TO_VK);
        UINT ch = LOWORD(MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR));
        if (ch >= 0x20) {
            k->dyn[0] = (wchar_t)(ULONG_PTR)CharUpperW((LPWSTR)(ULONG_PTR)ch);
            k->dyn[1] = 0;
        } else {
            GetKeyNameTextW((LONG)((k->id & 0xFF) << 16), k->dyn, 8);
        }
    }
}

/* ---------------------------------------------------------------------- */

static COLORREF mix(COLORREF a, COLORREF b, float t)
{
    if (t <= 0) return a;
    if (t >= 1) return b;
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
               (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

/* 1 = held, fading 1..0 after release, 0 = idle */
static float heat(int id, DWORD now)
{
    if (g_down[id]) return 1.0f;
    DWORD dt = now - g_upTick[id];
    if (g_upTick[id] && dt < FADE_MS) return 1.0f - (float)dt / FADE_MS;
    return 0.0f;
}

static BOOL any_fading(DWORD now)
{
    for (int id = 0; id < 512; id++)
        if (!g_down[id] && g_upTick[id] && now - g_upTick[id] < FADE_MS + TIMER_MS)
            return TRUE;
    return FALSE;
}

static HFONT make_font(int px, int weight)
{
    return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH, L"Segoe UI");
}

static void key_name(int id, wchar_t *buf, int n)
{
    buf[0] = 0;
    if (id == 0x045) { wcsncpy(buf, L"Pause", n); return; }
    if (id == 0x137) { wcsncpy(buf, L"Print Screen", n); return; }
    LONG lp = (LONG)(((id & 0xFF) << 16) | ((id & 0x100) ? (1 << 24) : 0));
    if (!GetKeyNameTextW(lp, buf, n) || !buf[0])
        _snwprintf(buf, n, L"Key 0x%03X", id);
}

static void paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) { EndPaint(hwnd, &ps); return; }

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    HBRUSH bg = CreateSolidBrush(COL_BG);
    FillRect(mem, &rc, bg);
    DeleteObject(bg);

    int statusH = H / 9 < 28 ? 28 : H / 9;
    int pad = W / 60 < 10 ? 10 : W / 60;
    float sx = (W - 2.0f * pad) / BOARD_W;
    float sy = (H - statusH - 2.0f * pad) / BOARD_H;
    float s = sx < sy ? sx : sy;
    if (s < 4) s = 4;
    float ox = (W - BOARD_W * s) / 2;
    float oy = pad + (H - statusH - 2 * pad - BOARD_H * s) / 2;
    int gap = (int)(s * 0.07f) < 2 ? 2 : (int)(s * 0.07f);
    int rad = (int)(s * 0.22f);

    HFONT fBig = make_font((int)(s * 0.38f), FW_SEMIBOLD);
    HFONT fSmall = make_font((int)(s * 0.24f), FW_NORMAL);
    SetBkMode(mem, TRANSPARENT);
    DWORD now = GetTickCount();

    for (int i = 0; i < NKEYS; i++) {
        const Key *k = &g_keys[i];
        float t = heat(k->id, now);
        HBRUSH fill = CreateSolidBrush(mix(COL_KEY, COL_HOT, t));
        COLORREF edgeCol = mix(COL_KEY_EDGE, COL_HOT_EDGE, t);

        int l = (int)(ox + k->x * s) + gap / 2;
        int tp = (int)(oy + k->y * s) + gap / 2;
        int r = (int)(ox + (k->x + k->w) * s) - gap / 2;
        int b = (int)(oy + (k->y + k->h) * s) - gap / 2;
        RECT text = { l, tp, r, b };

        if (k->id == ISO_ENTER) {
            /* L-shape: full-width top row, narrower lower part */
            int lb = (int)(ox + (k->x + 0.25f) * s) + gap / 2;
            int mid = (int)(oy + (k->y + 1) * s);
            HRGN top = CreateRoundRectRgn(l, tp, r + 1, mid + rad, rad, rad);
            HRGN bot = CreateRoundRectRgn(lb, mid - rad, r + 1, b + 1, rad, rad);
            CombineRgn(top, top, bot, RGN_OR);
            FillRgn(mem, top, fill);
            HBRUSH edge = CreateSolidBrush(edgeCol);
            FrameRgn(mem, top, edge, 1, 1);
            DeleteObject(edge);
            DeleteObject(top);
            DeleteObject(bot);
            text.bottom = mid;
        } else {
            HPEN pen = CreatePen(PS_SOLID, 1, edgeCol);
            HGDIOBJ op = SelectObject(mem, pen), ob = SelectObject(mem, fill);
            RoundRect(mem, l, tp, r, b, rad, rad);
            SelectObject(mem, op);
            SelectObject(mem, ob);
            DeleteObject(pen);
        }
        DeleteObject(fill);

        const wchar_t *lab = k->label ? k->label : k->dyn;
        SelectObject(mem, wcslen(lab) <= 1 ? fBig : fSmall);
        SetTextColor(mem, mix(COL_TEXT, COL_HOT_TEXT, t));
        DrawTextW(mem, lab, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    /* Status bar: names of everything currently held */
    wchar_t status[1024] = L"";
    int count = 0;
    for (int id = 0; id < 512; id++) {
        if (!g_down[id]) continue;
        wchar_t name[64];
        key_name(id, name, 64);
        if (count++) wcsncat(status, L"  +  ", 1023 - wcslen(status));
        wcsncat(status, name, 1023 - wcslen(status));
    }
    if (!count)
        wcscpy(status, L"Press any key \x2014 works even when this window isn't focused");

    HFONT fStatus = make_font(statusH * 45 / 100, FW_NORMAL);
    SelectObject(mem, fStatus);
    SetTextColor(mem, count ? COL_HOT_EDGE : COL_STATUS);
    RECT sr = { pad, H - statusH - pad / 2, W - pad, H - pad / 2 };
    DrawTextW(mem, status, -1, &sr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteObject(fBig);
    DeleteObject(fSmall);
    DeleteObject(fStatus);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

/* ---------------------------------------------------------------------- */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static int ticks;
    switch (msg) {
    case WM_CREATE: {
        HMENU sys = GetSystemMenu(hwnd, FALSE);
        AppendMenuW(sys, MF_SEPARATOR, 0, NULL);
        AppendMenuW(sys, MF_STRING, IDM_TOPMOST, L"Always on top");
        SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == IDM_TOPMOST) {
            g_topmost = !g_topmost;
            SetWindowPos(hwnd, g_topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                         0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            CheckMenuItem(GetSystemMenu(hwnd, FALSE), IDM_TOPMOST,
                          MF_BYCOMMAND | (g_topmost ? MF_CHECKED : MF_UNCHECKED));
            return 0;
        }
        break;
    case WM_TIMER: {
        DWORD now = GetTickCount();
        if (any_fading(now)) InvalidateRect(hwnd, NULL, FALSE);
        if (++ticks % 15 == 0) unstick_keys();
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 480;
        mm->ptMinTrackSize.y = 200;
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    (void)hPrev; (void)cmd;
    SetProcessDPIAware();
    load_layout_labels();

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = L"KeyVizWindow";
    RegisterClassExW(&wc);

    HDC screen = GetDC(NULL);
    int dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(NULL, screen);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"KeyViz \x2014 live keyboard",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             MulDiv(1100, dpi, 96), MulDiv(400, dpi, 96),
                             NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
    if (!g_hook) {
        MessageBoxW(NULL, L"Could not install the keyboard hook.", L"KeyViz", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    UnhookWindowsHookEx(g_hook);
    return 0;
}
