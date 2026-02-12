#include "PrecompiledHeader.h"
#include "BorderedTextBox.h"
#include "Utilities/WindowUtilities.h"
#include "Utilities/WndClassHolder.h"

constexpr intptr_t kBorderColor = COLOR_WINDOWFRAME + 1;
constexpr intptr_t kActiveBorderColor = COLOR_HIGHLIGHT + 1;
constexpr intptr_t kBackgroundColor = COLOR_WINDOW + 1;
static WndClassHolder s_BorderedTextBoxWindowClass;
static LRESULT CALLBACK BorderedTextBoxWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

constexpr DWORD kEditStyleMask = ES_CENTER | ES_RIGHT | ES_MULTILINE | ES_UPPERCASE | ES_LOWERCASE | ES_PASSWORD | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_OEMCONVERT | ES_READONLY | ES_WANTRETURN | ES_NUMBER;

enum class TextBoxFocusState
{
    None = 0,
    BorderMouseHover = 1 << 0,
    TextBoxMouseHover = 1 << 1,
    Focused = 1 << 2,
};

MAKE_BIT_OPERATORS_FOR_ENUM_CLASS(TextBoxFocusState)

BorderedTextBox::StaticInitializer::StaticInitializer()
{
    WNDCLASSEXW classDescription = {};
    classDescription.cbSize = sizeof(classDescription);
    classDescription.style = CS_HREDRAW | CS_VREDRAW;
    classDescription.lpfnWndProc = BorderedTextBoxWndProc;
    classDescription.hInstance = GetHInstance();
    classDescription.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    classDescription.hbrBackground = reinterpret_cast<HBRUSH>(kBackgroundColor);
    classDescription.lpszClassName = BorderedTextBox::WindowClassName;
    classDescription.cbWndExtra = sizeof(HWND) + sizeof(intptr_t);

    s_BorderedTextBoxWindowClass = RegisterClassExW(&classDescription);
    Assert(s_BorderedTextBoxWindowClass != 0);
}

BorderedTextBox::StaticInitializer::~StaticInitializer()
{
    s_BorderedTextBoxWindowClass = 0;
}

static HWND GetEditControl(HWND parent)
{
    return reinterpret_cast<HWND>(GetWindowLongPtrW(parent, 0));
}

static int GetBorderWidth(HWND hWnd)
{
    return GetDpiForWindow(hWnd) / 96;
}

static TextBoxFocusState GetFocusState(HWND hWnd)
{
    return static_cast<TextBoxFocusState>(GetWindowLongPtrW(hWnd, sizeof(HWND)));
}

static bool EnterFocusState(HWND hWnd, TextBoxFocusState state)
{
    auto oldState = GetFocusState(hWnd);
    if ((oldState & state) == TextBoxFocusState::None)
    {
        auto newState = oldState | state;
        SetWindowLongPtrW(hWnd, sizeof(HWND), static_cast<LONG_PTR>(newState));

        if (oldState == TextBoxFocusState::None)
            InvalidateRect(hWnd, 0, TRUE);

        return true;
    }

    return false;
}

static bool LeaveFocusState(HWND hWnd, TextBoxFocusState state)
{
    auto oldState = GetFocusState(hWnd);
    if ((oldState & state) != TextBoxFocusState::None)
    {
        auto newState = oldState & ~state;
        SetWindowLongPtrW(hWnd, sizeof(HWND), static_cast<LONG_PTR>(newState));

        if (newState == TextBoxFocusState::None)
            InvalidateRect(hWnd, 0, TRUE);

        return true;
    }

    return false;
}

static LRESULT CALLBACK TextBoxSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    switch (msg)
    {
        case WM_MOUSEMOVE:
            if (EnterFocusState(GetParent(hWnd), TextBoxFocusState::TextBoxMouseHover))
            {
                TRACKMOUSEEVENT trackEvent;
                trackEvent.cbSize = sizeof(trackEvent);
                trackEvent.dwFlags = TME_LEAVE;
                trackEvent.hwndTrack = hWnd;
                trackEvent.dwHoverTime = 0;
                TrackMouseEvent(&trackEvent);
            }
            break;

        case WM_MOUSELEAVE:
            LeaveFocusState(GetParent(hWnd), TextBoxFocusState::TextBoxMouseHover);
            break;
    }

    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static void RepaintTextBox(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hWnd, &ps);

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);

    const int width = clientRect.right;
    const int height = clientRect.bottom;
    auto borderWidth = GetBorderWidth(hWnd);
    auto borderBrush = GetFocusState(hWnd) != TextBoxFocusState::None ? reinterpret_cast<HBRUSH>(kActiveBorderColor) : reinterpret_cast<HBRUSH>(kBorderColor);

    constexpr RECT kZeroRect = { 0, 0, 0, 0 };
    RECT leftLine = { 0, 0, borderWidth, height };
    RECT topLine = { 0, 0, width, borderWidth };
    RECT rightLine = { width - borderWidth, 0, width, height };
    RECT bottomLine = { 0, height - borderWidth, width, height };
    RECT textBoxArea = { borderWidth, borderWidth, width - borderWidth, height - borderWidth };

    IntersectRectInPlace(&leftLine, ps.rcPaint);
    IntersectRectInPlace(&topLine, ps.rcPaint);
    IntersectRectInPlace(&rightLine, ps.rcPaint);
    IntersectRectInPlace(&bottomLine, ps.rcPaint);
    IntersectRectInPlace(&textBoxArea, ps.rcPaint);

    if (!IsEmptyRect(leftLine))
    {
        auto result = FillRect(dc, &leftLine, borderBrush);
        Assert(result != 0);
    }

    if (!IsEmptyRect(topLine))
    {
        auto result = FillRect(dc, &topLine, borderBrush);
        Assert(result != 0);
    }

    if (!IsEmptyRect(rightLine))
    {
        auto result = FillRect(dc, &rightLine, borderBrush);
        Assert(result != 0);
    }

    if (!IsEmptyRect(bottomLine))
    {
        auto result = FillRect(dc, &bottomLine, borderBrush);
        Assert(result != 0);
    }

    if (!IsEmptyRect(textBoxArea))
    {
        auto result = FillRect(dc, &textBoxArea, reinterpret_cast<HBRUSH>(kBackgroundColor));
        Assert(result != 0);
    }

    EndPaint(hWnd, &ps);
}

static LRESULT CALLBACK BorderedTextBoxWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            auto borderWidth = GetBorderWidth(hWnd);
            auto editControl = CreateWindowExW(0, WC_EDIT, createStruct->lpszName, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, borderWidth, borderWidth + 1, createStruct->cx - 2 * borderWidth, createStruct->cy - 2 * borderWidth - 1, hWnd, nullptr, GetHInstance(), nullptr);
            Assert(editControl != nullptr);

            auto result = SetWindowSubclass(editControl, TextBoxSubclassProc, 0, 0);
            Assert(result != FALSE);

            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE) | WS_EX_CONTROLPARENT);
            SetWindowLongPtrW(hWnd, 0, reinterpret_cast<LONG_PTR>(editControl));
            return 0;
        }

        case WM_SIZE:
        {
            auto borderWidth = GetBorderWidth(hWnd);
            auto result = SetWindowPos(GetEditControl(hWnd), nullptr, borderWidth, borderWidth + 1, LOWORD(lParam) - 2 * borderWidth, HIWORD(lParam) - 2 * borderWidth - 1, SWP_NOZORDER | SWP_NOACTIVATE);
            Assert(result != FALSE);
            return 0;
        }

        case WM_PAINT:
        {
            RepaintTextBox(hWnd);
            return 0;
        }
        
        case WM_MOUSEMOVE:
            if (EnterFocusState(hWnd, TextBoxFocusState::BorderMouseHover))
            {
                TRACKMOUSEEVENT trackEvent;
                trackEvent.cbSize = sizeof(trackEvent);
                trackEvent.dwFlags = TME_LEAVE;
                trackEvent.hwndTrack = hWnd;
                trackEvent.dwHoverTime = 0;
                TrackMouseEvent(&trackEvent);
            }
            break;

        case WM_MOUSELEAVE:
            LeaveFocusState(hWnd, TextBoxFocusState::BorderMouseHover);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 0)
            {
                if (HIWORD(wParam) == EN_SETFOCUS)
                {
                    EnterFocusState(hWnd, TextBoxFocusState::Focused);
                }
                else if (HIWORD(wParam) == EN_KILLFOCUS)
                {
                    LeaveFocusState(hWnd, TextBoxFocusState::Focused);
                }
            }
            break;

        case WM_STYLECHANGING:
        {
            if (wParam == GWL_STYLE)
            {
                auto styles = reinterpret_cast<STYLESTRUCT*>(lParam);

                auto oldEditStyle = styles->styleOld & kEditStyleMask;
                auto newEditStyle = styles->styleNew & kEditStyleMask;

                if (oldEditStyle != newEditStyle)
                {
                    auto editControl = GetEditControl(hWnd);
                    SetWindowLongPtrW(editControl, GWL_STYLE, GetWindowLongPtrW(editControl, GWL_STYLE) | newEditStyle);
                }
            }
            break;
        }

        case WM_GETTEXTLENGTH:
            return GetWindowTextLengthW(GetEditControl(hWnd));

        case WM_GETTEXT:
            return GetWindowTextW(GetEditControl(hWnd), reinterpret_cast<LPWSTR>(lParam), static_cast<int>(wParam));

        case WM_GETFONT:
        case WM_SETTEXT:
        case WM_SETFONT:
            return SendMessageW(GetEditControl(hWnd), msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
