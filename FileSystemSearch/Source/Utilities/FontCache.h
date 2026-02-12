#pragma once

#include "NonCopyable.h"

class FontCache : NonCopyable
{
public:
    ~FontCache();

    HFONT GetFontForDpi(uint32_t dpi) const
    {
        auto it = m_FontCache.find(dpi);
        if (it != m_FontCache.end())
            return it->second;

        auto font = CreateFontForDpi(dpi);
        m_FontCache.emplace(dpi, font);
        return font;
    }

private:
    HFONT CreateFontForDpi(uint32_t dpi) const;

private:
    mutable std::map<uint32_t, HFONT> m_FontCache;
    LOGFONTW m_LogFont;
};