#pragma once

#include "NonCopyable.h"
#include "HandleHolder.h"
#include "Event.h"
#include "SearchEngine.h"
#include "TestList.h"

namespace Testing
{
    struct GlobalTestContext : NonCopyable
    {
        GlobalTestContext();
        ~GlobalTestContext();

        inline std::wstring_view GetGlobalTestDirectory() const
        {
            return m_GlobalTestDirectory;
        }

    private:
        std::wstring m_GlobalTestDirectory;
    };

    struct TestDirectory : NonCopyable
    {
        TestDirectory(std::wstring_view testName);
        
        inline TestDirectory SubDirectory(std::wstring_view name) const
        {
            return TestDirectory(m_Path, name);
        }

        inline std::wstring_view view() const
        {
            return m_Path;
        }

        inline const wchar_t* c_str() const
        {
            return m_Path.c_str();
        }

    private:
        TestDirectory(std::wstring_view parentDir, std::wstring_view name);

    private:
        std::wstring m_Path;
    };

    struct TestFile
    {
        TestFile(const TestDirectory& testDirectory, std::wstring_view fileName, std::span<const char> fileContents);

        inline const std::wstring& GetPath() const
        {
            return m_Path;
        }

    private:
        std::wstring m_Path;
    };

    template <typename BaseClass>
    struct SearchTestImpl : BaseClass
    {
        inline SearchTestImpl(std::wstring_view testName, SearchFlags extraSearchFlags = SearchFlags::kNone) :
            BaseClass(testName),
            m_ExtraSearchFlags(extraSearchFlags)
        {
        }

        const Testing::TestDirectory& GetTestDirectory() const
        {   
            if (!m_TestDirectory)
                m_TestDirectory.emplace(BaseClass::TestName());

            return *m_TestDirectory;
        }

        std::vector<std::wstring> PerformTestSearch(const wchar_t* searchPattern, const wchar_t* searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan = std::numeric_limits<uint64_t>::max()) const;

    protected:
        mutable std::optional<Testing::TestDirectory> m_TestDirectory;
        SearchFlags m_ExtraSearchFlags;
    };

    using SearchTest = SearchTestImpl<FunctionalTest>;
}
