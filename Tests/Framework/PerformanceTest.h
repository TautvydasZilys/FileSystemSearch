#pragma once

#include "SearchEngineTypes.h"
#include "TestMacros.h"
#include "TestHelpers.h"
#include "Utilities/CompileTimeString.h"

namespace Testing
{
    struct PreparedPerformanceTestLayout
    {
        inline PreparedPerformanceTestLayout(std::wstring_view testName, std::span<const size_t> layoutSizes, std::span<const CompileTimeString<wchar_t, MAX_PATH>> testFiles, std::span<const std::wstring_view> testFileExtensions);
        ~PreparedPerformanceTestLayout();

        PreparedPerformanceTestLayout(PreparedPerformanceTestLayout&& other) noexcept;
        PreparedPerformanceTestLayout& operator=(PreparedPerformanceTestLayout&& other) noexcept;

        const Testing::TestDirectory& GetTestDirectory() const
        {
            return m_Layout->GetTestDirectory();
        }

        void InvalidateTestFileCache() const;

        std::vector<std::wstring> PerformTestSearch(const wchar_t* searchPattern, const wchar_t* searchString, SearchFlags searchFlags) const
        {
            return m_Layout->PerformTestSearch(searchPattern, searchString, searchFlags);
        }

    private:
        void GenerateLayoutRecursive(const TestDirectory& testDirectory, std::span<const size_t> layoutSizes, std::span<const CompileTimeString<wchar_t, MAX_PATH>> testFiles, std::span<const std::wstring_view> testFileExtensions, std::span<size_t> counters, size_t depth = 0);

    private:
        std::optional<SearchTestImpl<ITest>> m_Layout;
        std::vector<Testing::TestFile> m_GeneratedFiles;
    };

    struct PerformanceTestDataLayout
    {
        PerformanceTestDataLayout(std::wstring_view layoutName, std::span<const size_t> layoutSizes) :
            m_LayoutName(layoutName),
            m_LayoutSizes(layoutSizes),
            m_TestFileExtensions(kDefaultTestFileExtensions)
        {
        }

        PerformanceTestDataLayout(std::wstring_view layoutName, std::span<const size_t> layoutSizes, std::span<const CompileTimeString<wchar_t, MAX_PATH>> testFiles) :
            m_LayoutName(layoutName),
            m_LayoutSizes(layoutSizes),
            m_TestFiles(testFiles),
            m_TestFileExtensions(kDefaultTestFileExtensions)
        {
        }

        PerformanceTestDataLayout(std::wstring_view layoutName, std::span<const size_t> layoutSizes, std::span<const CompileTimeString<wchar_t, MAX_PATH>> testFiles, std::span<const std::wstring_view> testFileExtensions) :
            m_LayoutName(layoutName),
            m_LayoutSizes(layoutSizes),
            m_TestFiles(testFiles),
            m_TestFileExtensions(testFileExtensions)
        {
        }

        PreparedPerformanceTestLayout Prepare() const;

        bool operator<(const PerformanceTestDataLayout& other) const;
        bool operator==(const PerformanceTestDataLayout& other) const;

        std::wstring_view GetLayoutName() const { return m_LayoutName; }

    private:
        std::wstring_view m_LayoutName;
        std::span<const size_t> m_LayoutSizes;
        std::span<const CompileTimeString<wchar_t, MAX_PATH>> m_TestFiles;
        std::span<const std::wstring_view> m_TestFileExtensions;

        static constexpr std::array<std::wstring_view, 1> kDefaultTestFileExtensions = { L".txt" };
    };

    template <typename T, typename ElementType>
    struct IsStdArrayOf : std::false_type {};

    template <typename T, size_t N, typename ElementType>
    struct IsStdArrayOf<std::array<T, N>, ElementType> : std::is_same<T, ElementType> {};

    template <typename T, typename ElementType>
    concept StdArrayOf = IsStdArrayOf<std::remove_cvref_t<T>, ElementType>::value;

    template <typename T>
    struct IsCompileTimeStringW : std::false_type {};

    template <size_t N>
    struct IsCompileTimeStringW<CompileTimeStringW<N>> : std::true_type {};

    template <typename T>
    concept ACompileTimeStringW = IsCompileTimeStringW<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept PerformanceTest = requires
    {
        { T::SearchString } -> ACompileTimeStringW;
        { T::SearchFlags } -> std::same_as<const SearchFlags&>;
        { T::PerformanceTestDataLayout } -> std::same_as<const PerformanceTestDataLayout* const&>;
    };

    template <typename T>
    concept PerformanceTestFull = PerformanceTest<T> && requires
    {
        { T::SearchPattern } -> std::same_as<const wchar_t* const&>;
    };

    template <PerformanceTest T, SearchFlags ExtraSearchFlags>
    struct PerformanceTestWrapper
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

    struct PerformanceTestBase : ITest, RegisteredTest<PerformanceTestBase>
    {
        PerformanceTestBase(std::wstring_view testName, const struct PerformanceTestDataLayout* PerformanceTestDataLayout) :
            ITest(testName),
            m_PerformanceTestDataLayout(PerformanceTestDataLayout),
            m_MedianTime(NAN)
        {
        }

        virtual void Run(const PreparedPerformanceTestLayout& testLayout) const = 0;

        const PerformanceTestDataLayout* GetPerformanceTestDataLayout() const { return m_PerformanceTestDataLayout; }
        double GetMedianRunTime() const { return m_MedianTime; }

    protected:
        const PerformanceTestDataLayout* m_PerformanceTestDataLayout;
        mutable double m_MedianTime;

    private:
        friend void ReportPerformanceTestResults();
    };

    template <PerformanceTestFull T, CompileTimeStringW Name>
    class PerformanceTestT : PerformanceTestBase
    {
    public:
        PerformanceTestT() :
            PerformanceTestBase(Name.value, T::PerformanceTestDataLayout)
        {
        }

        void Run(const PreparedPerformanceTestLayout& testLayout) const final override
        {
            static constexpr size_t kIterations = 50;

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

    template <PerformanceTest T, CompileTimeStringW TestName>
    struct ContentPerformanceTestWithoutUtf8IgnoreCaseT :
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8>, TestName + L"_UTF8">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16>, TestName + L"_UTF16">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16>, TestName + L"_UTF8_UTF16">,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase>, TestName + L"_UTF16_IgnoreCase">,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UseDirectStorage">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF16_UseDirectStorage">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UTF16_UseDirectStorage">,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF16_IgnoreCase_UseDirectStorage">
    {
        static_assert((T::SearchFlags & (SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage)) == SearchFlags::kNone,
            "You may not specify any search flags that dictate how content is searched in a performance test. All variations are enumerated automatically");
    };

    template <PerformanceTest T, CompileTimeStringW TestName>
    struct ContentPerformanceTestT :
        ContentPerformanceTestWithoutUtf8IgnoreCaseT<T, TestName>,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase>, TestName + L"_UTF8_IgnoreCase">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase>, TestName + L"_UTF8_UTF16_IgnoreCase">,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_IgnoreCase_UseDirectStorage">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UTF16_IgnoreCase_UseDirectStorage">
    {
    };

    template <PerformanceTest T, CompileTimeStringW TestName>
    struct FileNamePerformanceTest :
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kNone>, TestName>,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kIgnoreCase>, TestName + L"_IgnoreCase">
    {
        static_assert((T::SearchFlags & SearchFlags::kIgnoreCase) == SearchFlags::kNone,
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
    
    template <PerformanceTest T, CompileTimeStringW TestName>
    struct PerformanceTestDefinition
    {
        std::conditional_t<
            (T::SearchFlags & SearchFlags::kSearchInFileContents) != SearchFlags::kNone,
            std::conditional_t<HasNonAsciiCharacters<T::SearchString>::value,
                ContentPerformanceTestWithoutUtf8IgnoreCaseT<T, TestName>,
                ContentPerformanceTestT<T, TestName>
            >,
            FileNamePerformanceTest<T, TestName>
        > value;
    };

    int RunAllPerformanceTests();
    void ReportPerformanceTestResults();
}

#define DEFINE_PERFORMANCE_TEST(TestName, ...) Testing::PerformanceTestDefinition<__VA_ARGS__, L#TestName> s_##TestName##_instance