#pragma once

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

static HINSTANCE GetHInstance()
{
    return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

inline int DipsToPixels(int dips, uint32_t dpi)
{
    return static_cast<int>(std::ceil(static_cast<int64_t>(dips) * dpi / 96.0));
}

inline std::wstring GetWindowTextW(HWND hWnd)
{
    auto searchStringLength = GetWindowTextLengthW(hWnd);

    std::wstring str;
    str.resize(searchStringLength);

    auto actualLength = GetWindowTextW(hWnd, &str[0], searchStringLength + 1);
    str.resize(actualLength);
    return str;
}

inline bool IsChecked(HWND hWnd)
{
    return SendMessageW(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
}