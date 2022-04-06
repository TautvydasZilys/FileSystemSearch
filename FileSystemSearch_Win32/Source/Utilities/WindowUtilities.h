#pragma once

#include "WindowPosition.h"

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

struct TextLineMeasurement
{
    SIZE size;
    size_t wrapPoint;
};

inline TextLineMeasurement MeasureLineSize(HDC hdc, std::wstring_view text, int maxWidth)
{
    SIZE extents;
    int characterCount;
    auto result = GetTextExtentExPointW(hdc, text.data(), static_cast<int>(text.length()), maxWidth, &characterCount, nullptr, &extents);
    Assert(result != FALSE);

    size_t wrapPoint;
    if (static_cast<int>(text.length()) > characterCount)
    {
        // GDI will wrap the text at the last space
        wrapPoint = text.substr(0, characterCount).find_last_of(L' ');

        // If no space is within bounds, it will clip the last word and continue with the next word on the next line
        if (wrapPoint == std::wstring_view::npos)
            wrapPoint = text.find_first_of(L' ', characterCount);

        if (wrapPoint != std::wstring_view::npos)
        {
            wrapPoint++;
        }
        else
        {
            wrapPoint = text.size();
        }
    }
    else
    {
        wrapPoint = text.size();
    }

    if (extents.cx > maxWidth)
        extents.cx = maxWidth;

    return { extents, wrapPoint };
}

struct TextMeasurement 
{
    SIZE size;
    size_t lineCount;
};

inline TextMeasurement MeasureTextSize(HDC hdc, std::wstring_view text, int maxWidth)
{
    TextMeasurement textMeasurement = {};

    while (!text.empty())
    {
        auto [lineSize, wrapPoint] = MeasureLineSize(hdc, text, maxWidth);

        textMeasurement.lineCount++;

        if (textMeasurement.size.cx < lineSize.cx)
            textMeasurement.size.cx = lineSize.cx;

        textMeasurement.size.cy += lineSize.cy;

        text = text.substr(wrapPoint);
    }

    return textMeasurement;
}

template <size_t N>
std::array<WindowPosition, N> CalculateWrappingTextBlockPositions(HDC hdc, const std::array<std::wstring, N>& strings, int maxWidth, int margin)
{
    std::array<WindowPosition, N> positions;

    {
        ::TEXTMETRIC textMetrics;
        auto result = GetTextMetricsW(hdc, &textMetrics);
        Assert(result != FALSE);

        const auto lineHeight = textMetrics.tmHeight + textMetrics.tmExternalLeading;

        POINT layoutCoord = { 0, 0 };
        for (size_t i = 0; i < N; i++)
        {
            auto [textSize, lineCount] = MeasureTextSize(hdc, strings[i], maxWidth);

            if (layoutCoord.x != 0 && (lineCount > 1 || textSize.cx + layoutCoord.x > maxWidth))
            {
                layoutCoord.x = 0;
                layoutCoord.y += lineHeight;
            }

            auto& rect = positions[i];
            rect.x = layoutCoord.x;
            rect.y = layoutCoord.y;
            rect.cx = textSize.cx;
            rect.cy = textSize.cy;

            if (lineCount > 1)
            {
                layoutCoord.x = 0;
                layoutCoord.y += rect.cy;
            }
            else
            {
                layoutCoord.x = rect.x + textSize.cx + margin;
            }
        }
    }

    return positions;
}

template <size_t N>
void AdjustWindowPositions(std::array<WindowPosition, N>& positions, POINT offset)
{
    for (auto& position : positions)
    {
        position.x += offset.x;
        position.y += offset.y;
    }
}
