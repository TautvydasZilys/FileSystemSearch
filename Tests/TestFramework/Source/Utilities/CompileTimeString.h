#pragma once

template <typename CharType, size_t N>
struct CompileTimeString
{
    consteval CompileTimeString()
    {
        std::fill_n(value, N, CharType('\0'));
    }

    consteval CompileTimeString(const CharType(&str)[N])
    {
        std::copy_n(str, N, value);
        EnsureNullTerminated();
    }

    template <size_t M>
    consteval CompileTimeString(const CharType(&str)[M])
    {
        static_assert(M <= N, "Cannot construct a CompileTimeString from a larger string literal");
        std::copy_n(str, M, value);
        std::fill_n(value + M, N - M, CharType('\0'));
        EnsureNullTerminated();
    }

    template <size_t M>
    consteval CompileTimeString(CompileTimeString<CharType, M> other)
    {
        static_assert(M <= N, "Cannot construct a CompileTimeString, from a larger CompileTimeString");
        std::copy_n(other.value, M, value);
        std::fill_n(value + M, N - M, CharType('\0'));
        EnsureNullTerminated();
    }

    template <size_t Start, size_t Count = N - Start - 1>
    consteval CompileTimeString<CharType, Count + 1> SubStr() const
    {
        static_assert(Start >= 0 && Start < Length, "Start index is out of bounds");
        static_assert(Start + Count <= Length, "Requested substring continues past the end of the original string");

        CompileTimeString<CharType, Count + 1> result;
        std::copy_n(value + Start, Count, result.value);
        result.value[Count] = CharType('\0');
        return result;
    }

    template <std::size_t M>
    consteval auto operator+(const CompileTimeString<CharType, M>& rhs) const
    {
        CompileTimeString<CharType, N + M - 1> result;
        std::copy_n(value, N - 1, result.value);
        std::copy_n(rhs.value, M, result.value + (N - 1));
        result.EnsureNullTerminated();
        return result;
    }

    template <std::size_t M>
    consteval auto operator+(const CharType(&rhs)[M]) const
    {
        CompileTimeString<CharType, N + M - 1> result;
        std::copy_n(value, N - 1, result.value);
        std::copy_n(rhs, M, result.value + (N - 1));
        result.EnsureNullTerminated();
        return result;
    }

    auto operator<=>(const CompileTimeString<CharType, N>&) const = default;

    static constexpr size_t Length = N - 1;

    consteval void EnsureNullTerminated()
    {
        if (value[N - 1] != CharType('\0'))
            throw "Input string must be null terminated!";
    }

    CharType value[N];

    static_assert(N > 0, "CompileTimeString<CharType> must have a non-zero size to fit a null terminator");
};

template <std::size_t N>
using CompileTimeStringW = CompileTimeString<wchar_t, N>;
