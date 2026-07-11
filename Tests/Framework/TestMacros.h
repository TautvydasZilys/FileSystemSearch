#pragma once
#include "TestHelpers.h"
#include "TestList.h"
#include <format>

#define TEST_IMPL(name, BaseClass, ...)                                                                         \
    struct Test_##name : public BaseClass                                                                       \
    {                                                                                                           \
        Test_##name() :                                                                                         \
            BaseClass(L#name, __VA_ARGS__)                                                                      \
        {                                                                                                       \
        }                                                                                                       \
        void Run() const final override;                                                                        \
    } Test_##name##_instance;                                                                                   \
    void Test_##name::Run() const

#define TEST(name) TEST_IMPL(name, Testing::RegisteredTest)

#define SEARCH_TEST(name)                                                                                       \
    template <SearchFlags ExtraSearchFlags>                                                                     \
    struct Test_##name : public Testing::SearchTest                                                             \
    {                                                                                                           \
        Test_##name(std::wstring_view testName) :                                                               \
            Testing::SearchTest(testName, ExtraSearchFlags)                                                     \
        {                                                                                                       \
        }                                                                                                       \
        void Run() const final override;                                                                        \
    };                                                                                                          \
    Test_##name<SearchFlags::kNone> Test_##name##Overlapped_instance(L#name ## "Overlapped");                   \
    Test_##name<SearchFlags::kUseDirectStorage> Test_##name##DirectStorage_instance(L#name ## "DirectStorage"); \
    template <SearchFlags ExtraSearchFlags>                                                                     \
    void Test_##name<ExtraSearchFlags>::Run() const

#define CHECK(condition, text)                                                                                  \
    do {                                                                                                        \
        if (!(condition))                                                                                       \
        {                                                                                                       \
           auto error = std::format(L"{}:{}: CHECK({}) failed: {}", __FILEW__, __LINE__, L#condition, text);    \
                                                                                                                \
           if (IsDebuggerPresent())                                                                             \
               __debugbreak();                                                                                  \
                                                                                                                \
           throw Testing::TestFailedException(error);                                                           \
        }                                                                                                       \
    } while (false)
