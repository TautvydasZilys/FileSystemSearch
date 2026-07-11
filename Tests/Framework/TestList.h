#pragma once
#include "NonCopyable.h"

namespace Testing
{
    template <typename TestType>
    void RegisterTest(TestType* test);

    template <typename TestType>
    int RunAllTests();

    void ReportPerformanceTestResults();

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

    struct FunctionalTest : ITest
    {
        FunctionalTest(std::wstring_view testName) :
            ITest(testName)
        {
            RegisterTest<FunctionalTest>(this);
        }

        virtual void Run() const = 0;

    private:
        FunctionalTest* next = nullptr;

        inline static FunctionalTest* s_FirstRegisteredTest;
        inline static FunctionalTest* s_LastRegisteredTest;

        friend void RegisterTest<FunctionalTest>(FunctionalTest* test);
        friend int RunAllTests<FunctionalTest>();
    };

    struct PerformanceTestBase : ITest
    {
        PerformanceTestBase(std::wstring_view testName) :
            ITest(testName),
            m_MedianTime(NAN)
        {
            RegisterTest<PerformanceTestBase>(this);
        }

        virtual void Run() const = 0;

        double GetMedianRunTime() const { return m_MedianTime; }

    protected:
        mutable double m_MedianTime;

    private:
        PerformanceTestBase* next = nullptr;

        inline static PerformanceTestBase* s_FirstRegisteredTest;
        inline static PerformanceTestBase* s_LastRegisteredTest;

        friend void RegisterTest<PerformanceTestBase>(struct PerformanceTestBase* test);
        friend int RunAllTests<PerformanceTestBase>();
        friend void ReportPerformanceTestResults();
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
