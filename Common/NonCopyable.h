#pragma once

struct NonCopyable
{
    NonCopyable() {}
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};
