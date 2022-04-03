#pragma once

inline int DipsToPixels(int dips, uint32_t dpi)
{
    return static_cast<int>(static_cast<int64_t>(dips) * dpi / 96.0);
}

