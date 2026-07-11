#pragma once

#include "SearchEngineTypes.h"
#include "TestMacros.h"
#include "TestHelpers.h"

namespace Testing
{
    template <size_t N>
    struct WStringLiteral
    {
        constexpr WStringLiteral()
        {
            std::fill_n(value, N, L'\0');
        }

        constexpr WStringLiteral(const wchar_t(&str)[N])
        {
            std::copy_n(str, N, value);
        }

        template <size_t M>
        constexpr WStringLiteral(const wchar_t(&str)[M])
        {
            static_assert(M <= N, "Cannot construct a WStringLiteral from a larger string literal");
            std::copy_n(str, M, value);
            std::fill_n(value + M, N - M, L'\0');
        }

        template <size_t M>
        constexpr WStringLiteral(WStringLiteral<M> other)
        {
            static_assert(M <= N, "Cannot construct a WStringLiteral from a larger WStringLiteral");
            std::copy_n(other.value, M, value);
            std::fill_n(value + M, N - M, L'\0');
        }

        template <size_t Start, size_t Count = N - Start - 1>
        constexpr WStringLiteral<Count + 1> SubStr() const
        {
            static_assert(Start >= 0 && Start < Length, "Start index is out of bounds");
            static_assert(Start + Count <= Length, "Requested substring continues past the end of the original string");

            WStringLiteral<Count + 1> result;
            std::copy_n(value + Start, Count, result.value);
            result.value[Count] = L'\0';
            return result;
        }

        wchar_t value[N];
        
        static constexpr size_t Length = N - 1;
        static_assert(N > 0, "WStringLiteral must have a non-zero size to fit a null terminator");
    };

    template <std::size_t N, std::size_t M>
    constexpr auto operator+(const WStringLiteral<N>& lhs, const WStringLiteral<M>& rhs)
    {
        WStringLiteral<N + M - 1> result;
        std::copy_n(lhs.value, N - 1, result.value);
        std::copy_n(rhs.value, M, result.value + (N - 1));
        return result;
    }

    template <std::size_t N, std::size_t M>
    constexpr auto operator+(const WStringLiteral<N>& lhs, const wchar_t (&rhs)[M])
    {
        WStringLiteral<N + M - 1> result;
        std::copy_n(lhs.value, N - 1, result.value);
        std::copy_n(rhs, M, result.value + (N - 1));
        return result;
    }

    template <typename T, typename ElementType>
    struct IsStdArrayOf : std::false_type {};

    template <typename T, size_t N, typename ElementType>
    struct IsStdArrayOf<std::array<T, N>, ElementType> : std::is_same<T, ElementType> {};

    template <typename T, typename ElementType>
    concept StdArrayOf = IsStdArrayOf<std::remove_cvref_t<T>, ElementType>::value;

    template <typename T>
    struct IsStdArrayOfWStringLiteral : std::false_type {};

    template <size_t N, size_t M>
    struct IsStdArrayOfWStringLiteral<std::array<WStringLiteral<M>, N>> : std::true_type {};

    template <typename T>
    concept StdArrayOfWStringLiteral = IsStdArrayOfWStringLiteral<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept PerformanceTest = requires
    {
        { T::SearchString } -> std::same_as<const wchar_t* const&>;
        { T::SearchFlags } -> std::same_as<const SearchFlags&>;
        { T::LayoutSizes } -> StdArrayOf<size_t>;
    };

    template <typename T>
    concept PerformanceTestFull = PerformanceTest<T> && requires
    {
        { T::SearchPattern } -> std::same_as<const wchar_t* const&>;
        { T::TestFileExtensions } -> StdArrayOf<std::wstring_view>;
        { T::TestFiles } -> StdArrayOfWStringLiteral;
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

        static constexpr auto GetTestFileExtensions()
        {
            if constexpr (requires { { T::TestFileExtensions } -> StdArrayOf<std::wstring_view>; })
            {
                return T::TestFileExtensions;
            }
            else
            {
                static_assert(!requires { T::TestFileExtensions; }, "T::SearchPattern must be a std::array<std::wstring_view> if it exists");
                return std::array<std::wstring_view, 1> { L".txt" };
            }
        }

        static constexpr auto GetTestFiles()
        {
            if constexpr (requires { { T::TestFiles } -> StdArrayOfWStringLiteral; })
            {
                return T::TestFiles;
            }
            else
            {
                static_assert(!requires { T::TestFiles; }, "T::TestFiles must be a std::array<WStringLiteral> if it exists");
                return std::array<WStringLiteral<1>, 0>{};
            }
        }

    public:
        static constexpr const wchar_t* SearchString = T::SearchString;
        static constexpr SearchFlags SearchFlags = T::SearchFlags | ExtraSearchFlags;
        static constexpr auto LayoutSizes = T::LayoutSizes;

        // Optional properties
        static constexpr const wchar_t* SearchPattern = GetSearchPattern();
        static constexpr auto TestFileExtensions = GetTestFileExtensions();
        static constexpr auto TestFiles = GetTestFiles();
    };

    template <PerformanceTestFull T, WStringLiteral Name>
    class PerformanceTestT : PerformanceTestBase
    {
    public:
        PerformanceTestT() :
            PerformanceTestBase(Name.value)
        {
        }

        void Run() const final override
        {
            static constexpr size_t kIterations = 10;

            std::vector<double> measurements;
            measurements.resize(kIterations);

            LARGE_INTEGER frequency;
            QueryPerformanceFrequency(&frequency);

            SearchTestImpl<ITest> test(Name.value);
            PrepareTest(test.GetTestDirectory());

            for (size_t i = 0; i < kIterations; i++)
            {

                LARGE_INTEGER start, end;

                {
                    QueryPerformanceCounter(&start);
                    auto foundPaths = test.PerformTestSearch(T::SearchPattern, T::SearchString, T::SearchFlags);
                    QueryPerformanceCounter(&end);
                }

                measurements[i] = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
            }

            test.GetTestDirectory().Remove();

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

    private:
        static constexpr std::wstring_view GetFileExtension(size_t fileIndex)
        {
            return T::TestFileExtensions[fileIndex % T::TestFileExtensions.size()];
        }

        static void PrepareTest(const TestDirectory& testDirectory)
        {
            std::array<size_t, T::LayoutSizes.size()> counters = {};
            GenerateLayoutRecursive(testDirectory, counters);
        }

        static constexpr const wchar_t kFileNameCharacters[] = L"0123abcdABCD";

        static constexpr wchar_t GetFileNameCharacter(size_t index)
        {
            return kFileNameCharacters[index % (std::size(kFileNameCharacters) - 1)];
        }

        static constexpr std::array<wchar_t, 12> MakeBaseFileName(size_t index)
        {
            static const size_t kPrimes[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };
            std::array<wchar_t, 12> result;

            static_assert(std::size(kPrimes) == std::size(result), "Sizes must match!");

            index += 201911;

            for (size_t i = 0; i < result.size(); i++)
            {
                index = (index * kPrimes[i]) % 1004167;
                result[i] = GetFileNameCharacter(index);
            }

            return result;
        }

        template <size_t Depth = 0>
        static void GenerateLayoutRecursive(const TestDirectory& testDirectory, std::array<size_t, T::LayoutSizes.size()>& counters)
        {
            constexpr size_t FileDepth = T::LayoutSizes.size() - 1;
            for (size_t i = 0; i < T::LayoutSizes[FileDepth]; i++)
            {
                auto fileIndex = counters[FileDepth]++;
                const auto baseFileName = MakeBaseFileName(293 * fileIndex);
                const auto fileExtension = GetFileExtension(fileIndex);
                
                std::wstring fileName(&baseFileName[0], baseFileName.size());
                fileName += fileExtension;

                if constexpr (std::size(T::TestFiles) > 0)
                {
                    Testing::TestFile(testDirectory, fileName, T::TestFiles[fileIndex % std::size(T::TestFiles)].value);
                }
                else
                {
                    Testing::TestFile(testDirectory, fileName, std::span<const char>("x", 1));
                }
            }

            if constexpr (Depth != T::LayoutSizes.size() - 1)
            {
                for (size_t i = 0; i < T::LayoutSizes[Depth]; i++)
                {
                    auto folderIndex = counters[Depth]++;
                    auto folderName = MakeBaseFileName(593 * folderIndex);
                    auto subDir = testDirectory.SubDirectory(std::wstring_view(&folderName[0], folderName.size()));
                    GenerateLayoutRecursive<Depth + 1>(subDir, counters);
                }
            }
        }
    };

    template <PerformanceTest T, WStringLiteral TestName>
    struct ContentPerformanceTestT :
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8>, TestName + L"_UTF8">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16>, TestName + L"_UTF16">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16>, TestName + L"_UTF8_UTF16">,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase>, TestName + L"_UTF8_IgnoreCase">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase>, TestName + L"_UTF16_IgnoreCase">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase>, TestName + L"_UTF8_UTF16_IgnoreCase">,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UseDirectStorage">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF16_UseDirectStorage">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UTF16_UseDirectStorage">,

        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_IgnoreCase_UseDirectStorage">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF16_IgnoreCase_UseDirectStorage">,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage>, TestName + L"_UTF8_UTF16_IgnoreCase_UseDirectStorage">
    {
        static_assert((T::SearchFlags & (SearchFlags::kSearchContentsAsUtf8 | SearchFlags::kSearchContentsAsUtf16 | SearchFlags::kIgnoreCase | SearchFlags::kUseDirectStorage)) == SearchFlags::kNone, 
            "You may not specify any search flags that dictate how content is searched in a performance test. All variations are enumerated automatically");
    };

    template <PerformanceTest T, WStringLiteral TestName>
    struct FileNamePerformanceTest :
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kNone>, TestName>,
        PerformanceTestT<PerformanceTestWrapper<T, SearchFlags::kIgnoreCase>, TestName + L"_IgnoreCase">
    {
        static_assert((T::SearchFlags & SearchFlags::kIgnoreCase) == SearchFlags::kNone,
            "You may not specify IgnoreCase flag in a performance test. Both case preserving and case ignoring cases are automatically tested.");
    };
    
    template <PerformanceTest T, WStringLiteral TestName>
    struct PerformanceTestDefinition
    {
        std::conditional_t<
            (T::SearchFlags & SearchFlags::kSearchInFileContents) != SearchFlags::kNone,
            ContentPerformanceTestT<T, TestName>,
            FileNamePerformanceTest<T, TestName>
        > value;
    };
}

#define DEFINE_PERFORMANCE_TEST(TestName, ...) Testing::PerformanceTestDefinition<__VA_ARGS__, L#TestName> s_##TestName##_instance