#include "PrecompiledHeader.h"
#include "TestMacros.h"
#include "TestHelpers.h"
#include "PerformanceTest.h"

struct WideFileNameSearchLittleMatches
{
    static constexpr const wchar_t* SearchString = L"2bcB";
    static constexpr SearchFlags SearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively;
    static constexpr std::array<size_t, 2> LayoutSizes = { 20, 100 };
};

DEFINE_PERFORMANCE_TEST(WideFileNameSearchLittleMatches)

struct WideFileNameSearchLotsOfMatches
{
    static constexpr const wchar_t* SearchString = L"b";
    static constexpr SearchFlags SearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively;
    static constexpr std::array<size_t, 2> LayoutSizes = { 20, 100 };
};

DEFINE_PERFORMANCE_TEST(WideFileNameSearchLotsOfMatches)

struct DeepFileNameSearchLittleMatches
{
    static constexpr const wchar_t* SearchString = L"2bcB";
    static constexpr SearchFlags SearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively;
    static constexpr std::array<size_t, 5> LayoutSizes = { 4, 4, 4, 4, 5 };
};

DEFINE_PERFORMANCE_TEST(DeepFileNameSearchLittleMatches)

struct DeepFileNameSearchLotsOfMatches
{
    static constexpr const wchar_t* SearchString = L"b";
    static constexpr SearchFlags SearchFlags = SearchFlags::kSearchForFiles | SearchFlags::kSearchInFileName | SearchFlags::kSearchRecursively;
    static constexpr std::array<size_t, 5> LayoutSizes = { 4, 4, 4, 4, 5 };
};

DEFINE_PERFORMANCE_TEST(DeepFileNameSearchLotsOfMatches)
