#pragma once

#include "HandleHolder.h"
#include "NonCopyable.h"
#include "SearchEngineTypes.h"
#include "Utilities/HwndHolder.h"

class SearchResultWindow : NonCopyable
{
public:
    struct StaticInitializer : NonCopyable
    {
        StaticInitializer();
        ~StaticInitializer();
    };

    static void Spawn(const std::wstring& searchPath, const std::wstring& searchPattern, const std::wstring& searchString, SearchFlags searchFlags, uint64_t ignoreFilesLargerThan);

private:
    SearchResultWindow();
    ~SearchResultWindow();

    operator HWND() const
    {
        return m_Hwnd;
    }

private:
    static LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate(HWND hWnd);

private:
    HwndHolder m_Hwnd;
    ThreadHandleHolder m_Thread;
};