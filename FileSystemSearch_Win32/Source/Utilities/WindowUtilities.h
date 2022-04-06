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

inline int MeasureTextHeight(HDC dc, HFONT font, const std::wstring& textStr, int width)
{
    std::wstring_view text(textStr);
    int textHeight = 0;

    auto oldFont = SelectObject(dc, font);

    while (!text.empty())
    {
        SIZE extents;
        int characterCount;
        auto result = GetTextExtentExPointW(dc, text.data(), static_cast<int>(text.length()), width, &characterCount, nullptr, &extents);
        Assert(result != FALSE);

        textHeight += extents.cy;
        if (characterCount == 0)
            break;

        if (static_cast<int>(text.length()) > characterCount)
        {
            // GDI will wrap the text at the last space
            auto wrapPoint = text.substr(0, characterCount).find_last_of(L' ');

            // If no space is within bounds, it will clip the last word and continue with the next word on the next line
            if (wrapPoint == std::wstring_view::npos)
                wrapPoint = text.find_first_of(L' ', characterCount);

            if (wrapPoint != std::wstring_view::npos)
            {
                text = text.substr(wrapPoint + 1);
            }
            else
            {
                text = std::wstring_view();
            }
        }
        else
        {
            text = text.substr(characterCount);
        }
    }

    SelectObject(dc, oldFont);
    return textHeight;
}