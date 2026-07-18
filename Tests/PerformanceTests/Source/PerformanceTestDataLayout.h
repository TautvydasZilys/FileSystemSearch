#pragma once
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
}