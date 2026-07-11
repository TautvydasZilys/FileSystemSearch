#pragma once
#include "NonCopyable.h"

namespace Testing
{
    void RegisterTest(struct ITest* test);
    int RunAllTests();

    struct ITest : NonCopyable
    {
        ITest(std::wstring_view testName) :
            m_TestName(testName)
        {
            RegisterTest(this);
        }

        std::wstring_view TestName() const { return m_TestName; }
        virtual void Run() const = 0;

    private:
        ITest* next = nullptr;
        std::wstring_view m_TestName;

        friend void RegisterTest(ITest* test);
        friend int RunAllTests();
    };

    struct TestFailedException
    {
        TestFailedException(std::wstring text) :
            m_Text(text)
        {
        }

        inline std::wstring_view Text() const
        {
            return m_Text;
        }

    private:
        std::wstring m_Text;
    };
}
