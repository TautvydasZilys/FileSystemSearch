#pragma once
#include "TestHelpers.h"
#include "TestList.h"
#include <format>

#define TEST_IMPL(name, BaseClass)                                                                          \
    struct Test_##name : public BaseClass                                                                   \
    {                                                                                                       \
        std::wstring_view TestName() const override { return L#name; }                                      \
        Test_##name() { Testing::RegisterTest(this); }                                                      \
        void Run() const override;                                                                          \
    } Test_##name##_instance;                                                                               \
    void Test_##name::Run() const

#define TEST(name) TEST_IMPL(name, Testing::ITest)

#define SEARCH_TEST(name) TEST_IMPL(name, Testing::SearchTest)

#define CHECK(condition, text)                                                                              \
    do {                                                                                                    \
        if (!(condition))                                                                                   \
        {                                                                                                   \
           auto error = std::format(L"{}:{}: CHECK({}) failed: {}", __FILEW__, __LINE__, L#condition, text); \
                                                                                                            \
           if (IsDebuggerPresent())                                                                         \
               __debugbreak();                                                                              \
                                                                                                            \
           throw Testing::TestFailedException(error);                                                       \
        }                                                                                                   \
    } while (false)
