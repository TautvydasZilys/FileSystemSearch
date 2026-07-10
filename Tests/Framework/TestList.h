#pragma once
#include "NonCopyable.h"

namespace Testing
{
    struct ITest : NonCopyable
    {
        virtual std::wstring_view TestName() const = 0;
        virtual void Run() const = 0;

    private:
        ITest* next = nullptr;

        friend void RegisterTest(ITest* test);
        friend int RunAllTests();
    };

    void RegisterTest(ITest* test);
    int RunAllTests();

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
