#pragma once
#include "NonCopyable.h"

namespace BorderedTextBox
{
    constexpr const wchar_t WindowClassName[] = L"FileSystemSearch_BorderedTextBox";

    struct StaticInitializer : NonCopyable
    {
        StaticInitializer();
        ~StaticInitializer();
    };
};