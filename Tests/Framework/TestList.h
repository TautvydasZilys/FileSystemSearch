#pragma once
#include "NonCopyable.h"

namespace Testing
{
    int RunAllFunctionalTests();

    struct ITest : NonCopyable
    {
        ITest(std::wstring_view testName) :
            m_TestName(testName)
        {
        }

        std::wstring_view TestName() const { return m_TestName; }

    private:
        std::wstring_view m_TestName;
    };
    
    template <typename T>
    struct RegisteredTest : NonCopyable
    {
        RegisteredTest()
        {
            static_assert(std::is_base_of_v<RegisteredTest<T>, T>, "RegisteredTest<T> must be a base of T");

            if (s_FirstRegisteredTest != nullptr)
                next = s_FirstRegisteredTest;

            s_FirstRegisteredTest = static_cast<T*>(this);
        }

        inline static T* GetFirstTest()
        {
            return s_FirstRegisteredTest;
        }

        inline T* GetNextTest() const
        {
            return next;
        }

    private:
        T* next = nullptr;
        inline static T* s_FirstRegisteredTest;
    };

    struct FunctionalTest : RegisteredTest<FunctionalTest>, ITest
    {
        FunctionalTest(std::wstring_view testName) :
            ITest(testName)
        {
        }

        virtual void Run() const = 0;
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
