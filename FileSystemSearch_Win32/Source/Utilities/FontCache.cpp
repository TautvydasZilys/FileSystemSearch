#include "PrecompiledHeader.h"
#include "FontCache.h"

FontCache::~FontCache()
{
    for (const auto& font : m_FontCache)
        DeleteObject(font.second);
}

HFONT FontCache::CreateFontForDpi(uint32_t dpi) const
{
    NONCLIENTMETRICS nonClientMetrics;
    nonClientMetrics.cbSize = sizeof(nonClientMetrics);

    auto result = SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(nonClientMetrics), &nonClientMetrics, 0, dpi);
    Assert(result != FALSE);

    auto hFont = CreateFontIndirectW(&nonClientMetrics.lfStatusFont);
    Assert(hFont != nullptr);

    return hFont;
}
