#pragma once
#include "PerformanceTest.h"
#include "PerformanceTestDataLayout.h"

namespace Testing
{
    template <typename T>
    struct IsCompileTimeStringW : std::false_type {};

    template <size_t N>
    struct IsCompileTimeStringW<CompileTimeStringW<N>> : std::true_type {};

    template <typename T>
    concept ACompileTimeStringW = IsCompileTimeStringW<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept PerformanceIntegrationTest = requires
    {
        { T::SearchString } -> ACompileTimeStringW;
        { T::SearchFlags } -> std::same_as<const SearchFlags&>;
        { T::PerformanceTestDataLayout } -> std::same_as<const PerformanceTestDataLayout* const&>;
    };

    template <typename T>
    concept PerformanceIntegrationTestFull = PerformanceIntegrationTest<T> && requires
    {
        { T::SearchPattern } -> std::same_as<const wchar_t* const&>;
    };

    template <PerformanceIntegrationTest T, SearchFlags ExtraSearchFlags>
    struct PerformanceIntegrationTestWrapper
    {
    private:
        static constexpr const wchar_t* GetSearchPattern()
        {
            if constexpr (requires { { T::SearchPattern } -> std::same_as<const wchar_t*>; })
            {
                return T::SearchPattern;
            }
            else
            {
                static_assert(!requires { T::SearchPattern; }, "T::SearchPattern must be a const wchar_t* if it exists");
                return L"*";
            }
        }

    public:
        static constexpr auto SearchString = T::SearchString;
        static constexpr SearchFlags SearchFlags = T::SearchFlags | ExtraSearchFlags;
        static constexpr auto PerformanceTestDataLayout = T::PerformanceTestDataLayout;

        // Optional properties
        static constexpr const wchar_t* SearchPattern = GetSearchPattern();
    };

    struct PerformanceIntegrationTestBase : ITest, RegisteredTest<PerformanceIntegrationTestBase>
    {
        PerformanceIntegrationTestBase(std::wstring_view testName, const struct PerformanceTestDataLayout* PerformanceTestDataLayout) :
            ITest(testName),
            m_PerformanceTestDataLayout(PerformanceTestDataLayout),
            m_MedianTime(NAN)
        {
        }

        virtual void Run(const PreparedPerformanceTestLayout& testLayout) const = 0;

        const PerformanceTestDataLayout* GetPerformanceTestDataLayout() const { return m_PerformanceTestDataLayout; }
        double GetMedianRunTime() const { return m_MedianTime; }

    protected:
        std::vector<std::wstring> LoadPerformanceTestResultForComparison(std::wstring_view testDirectory) const;
        void SavePerformanceTestResultForComparison(std::wstring_view testDirectory, const std::vector<std::wstring>& foundPaths) const;

    private:
        std::wstring GetPerformanceTestResultForComparisonPath() const;

    protected:
        const PerformanceTestDataLayout* m_PerformanceTestDataLayout;
        mutable double m_MedianTime;

    private:
        friend void ReportPerformanceTestResults();
    };

    template <PerformanceIntegrationTestFull T, CompileTimeStringW Name>
    class PerformanceIntegrationTestT : PerformanceIntegrationTestBase
    {
    public:
        PerformanceIntegrationTestT() :
            PerformanceIntegrationTestBase(Name.value, T::PerformanceTestDataLayout)
        {
        }

        void Run(const PreparedPerformanceTestLayout& testLayout) const final override
        {
            static constexpr bool kSavePerformanceTestResultForComparison = false;
            static constexpr size_t kIterations = kSavePerformanceTestResultForComparison ? 1 : 50;

            std::vector<std::wstring> expectedPaths;
            if constexpr (!kSavePerformanceTestResultForComparison)
                expectedPaths = LoadPerformanceTestResultForComparison(testLayout.GetTestDirectory().view());

            std::vector<double> measurements;
            measurements.resize(kIterations);

            LARGE_INTEGER frequency;
            QueryPerformanceFrequency(&frequency);

            for (size_t i = 0; i < kIterations; i++)
            {
                LARGE_INTEGER start, end;
                testLayout.InvalidateTestFileCache();

                {
                    QueryPerformanceCounter(&start);
                    auto foundPaths = testLayout.PerformTestSearch(T::SearchPattern, T::SearchString.value, T::SearchFlags);
                    QueryPerformanceCounter(&end);

                    std::sort(foundPaths.begin(), foundPaths.end());

                    if constexpr (kSavePerformanceTestResultForComparison)
                    {
                        SavePerformanceTestResultForComparison(testLayout.GetTestDirectory().view(), foundPaths);
                    }
                    else
                    {
                        CHECK(foundPaths.size() == expectedPaths.size(), std::format(L"Found {} paths, but expected {} paths at iteration #{}", foundPaths.size(), expectedPaths.size(), i));

                        for (size_t j = 0; j < foundPaths.size(); j++)
                            CHECK(foundPaths[j] == expectedPaths[j], std::format(L"Found path '{}' does not match expected path '{}' at iteration #{}", foundPaths[j], expectedPaths[j], i));
                    }
                }

                measurements[i] = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
            }

            std::sort(measurements.begin(), measurements.end());

            if (kIterations % 2 == 0)
            {
                m_MedianTime = (measurements[kIterations / 2 - 1] + measurements[kIterations / 2]) / 2.0;
            }
            else
            {
                m_MedianTime = measurements[kIterations / 2];
            }
        }
    };

    template <PerformanceIntegrationTest T, CompileTimeStringW TestName>
    struct ContentPerformanceTestWithoutUtf8IgnoreCaseT :
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8>, TestName + L"_UTF8">,
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf16>, TestName + L"_UTF16">,
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16>, TestName + L"_UTF8_UTF16">,

        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase>, TestName + L"_UTF16_IgnoreCase">,

        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UseDirectStorage">,
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF16_UseDirectStorage">,
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UTF16_UseDirectStorage">,

        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF16_IgnoreCase_UseDirectStorage">
    {
        static_assert((T::SearchFlags& (SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage)) == SearchFlags::kNone,
            "You may not specify any search flags that dictate how content is searched in a performance test. All variations are enumerated automatically");
    };

    template <PerformanceIntegrationTest T, CompileTimeStringW TestName>
    struct ContentPerformanceIntegrationTestT :
        ContentPerformanceTestWithoutUtf8IgnoreCaseT<T, TestName>,

        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase>, TestName + L"_UTF8_IgnoreCase">,
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase>, TestName + L"_UTF8_UTF16_IgnoreCase">,

        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_IgnoreCase_UseDirectStorage">,
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UTF16_IgnoreCase_UseDirectStorage">
    {
    };

    template <PerformanceIntegrationTest T, CompileTimeStringW TestName>
    struct FileNamePerformanceTest :
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kNone>, TestName>,
        PerformanceIntegrationTestT<PerformanceIntegrationTestWrapper<T, SearchFlags::kIgnoreCase>, TestName + L"_IgnoreCase">
    {
        static_assert((T::SearchFlags& SearchFlags::kIgnoreCase) == SearchFlags::kNone,
            "You may not specify IgnoreCase flag in a performance test. Both case preserving and case ignoring cases are automatically tested.");
    };

    template <CompileTimeStringW SearchString>
    struct HasNonAsciiCharacters
    {
        static constexpr bool value = []()
        {
            for (size_t i = 0; i < SearchString.Length; i++)
            {
                if (SearchString.value[i] > 127)
                    return true;
            }

            return false;
        }();
    };

    template <PerformanceIntegrationTest T, CompileTimeStringW TestName>
    struct PerformanceIntegrationTestDefinition
    {
        std::conditional_t<
            (T::SearchFlags& SearchFlags::kSearchInFileContents) != SearchFlags::kNone,
            std::conditional_t<HasNonAsciiCharacters<T::SearchString>::value,
            ContentPerformanceTestWithoutUtf8IgnoreCaseT<T, TestName>,
            ContentPerformanceIntegrationTestT<T, TestName>
            >,
            FileNamePerformanceTest<T, TestName>
        > value;
    };
}

#define DEFINE_PERFORMANCE_INTEGRATION_TEST(TestName, ...) Testing::PerformanceIntegrationTestDefinition<__VA_ARGS__, L#TestName> s_##TestName##_instance
