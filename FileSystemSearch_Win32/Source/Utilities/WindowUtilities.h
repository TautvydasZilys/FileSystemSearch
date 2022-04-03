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