#include "PrecompiledHeader.h"
#include "SearchEngine.h"
#include "SearchWindow.h"
#include "Utilities/FontCache.h"
#include "Utilities/WindowUtilities.h"

static SearchWindow* s_Instance;
static HINSTANCE GetHInstance();

constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
constexpr DWORD kWindowStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
constexpr int kWindowClientWidth = 409;
constexpr int kWindowClientHeight = 639;

constexpr intptr_t kBackgroundColor = COLOR_WINDOW + 1;

enum class ByteUnit : uint64_t
{
    B = 1,
    KB = 1024 * B,
    MB = 1024 * KB,
    GB = 1024 * MB,
    TB = 1024 * GB,
};

struct NamedByteUnit
{
    constexpr NamedByteUnit(ByteUnit unit, const wchar_t* name) :
        unit(unit), name(name)
    {
    }

    ByteUnit unit;
    const wchar_t* name;
};

constexpr NamedByteUnit kByteUnits[] =
{
    { ByteUnit::B, L"B" },
    { ByteUnit::KB, L"KB" },
    { ByteUnit::MB, L"MB" },
    { ByteUnit::GB, L"GB" },
    { ByteUnit::TB, L"TB" },
};

SearchWindow::SearchWindow(const FontCache& fontCache, int nCmdShow) :
    m_FontCache(fontCache)
{
    Assert(s_Instance == nullptr); // Current design dictates only one Search Window gets spawned per app instance
    s_Instance = this;

    WNDCLASSEXW classDescription = {};
    classDescription.cbSize = sizeof(classDescription);
    classDescription.style = CS_HREDRAW | CS_VREDRAW;
    classDescription.lpfnWndProc = SearchWindow::WndProc;
    classDescription.hInstance = GetHInstance();
    classDescription.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    classDescription.hbrBackground = reinterpret_cast<HBRUSH>(kBackgroundColor);
    classDescription.lpszClassName = L"SearchWindow";

    m_WindowClass = RegisterClassExW(&classDescription);
    Assert(m_WindowClass != 0);

    auto hwnd = CreateWindowExW(kWindowExStyle, m_WindowClass, L"File System Search - Search", kWindowStyle, CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, GetHInstance(), nullptr);
    Assert(m_Hwnd == hwnd);

    auto wasVisible = ShowWindow(m_Hwnd, nCmdShow);
    Assert(wasVisible == FALSE);
}

SearchWindow::~SearchWindow()
{
    Assert(s_Instance == this);
    s_Instance = nullptr;
}

LRESULT SearchWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            s_Instance->OnCreate(hWnd);

            auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            s_Instance->AdjustSearchWindowPlacement(createStruct->x, createStruct->y, GetDpiForWindow(hWnd));
            return 0;
        }

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_CTLCOLORSTATIC:
            return kBackgroundColor;

        case WM_DPICHANGED:
        {
            auto targetRect = reinterpret_cast<RECT*>(lParam);
            s_Instance->AdjustSearchWindowPlacement(targetRect->left, targetRect->top, HIWORD(wParam));

            return 0;
        }

        case WM_COMMAND:
            if (wParam == IDOK || (LOWORD(wParam) == m_SearchButton && HIWORD(wParam) == BN_CLICKED))
                s_Instance->SearchButtonClicked();
            
            break;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SearchWindow::OnCreate(HWND hWnd)
{
    m_Hwnd = hWnd;

    for (size_t i = 0; i < m_ControlCount; i++)
        m_Controls[i] = kControls[i].Create(hWnd, i);
    
    // Restrict to numbers only
    HWND ignoreFilesLargerThanTextBox = m_Controls[m_IgnoreFilesLargerThanTextBox];
    SetWindowLongPtrW(ignoreFilesLargerThanTextBox, GWL_STYLE, GetWindowLongPtrW(ignoreFilesLargerThanTextBox, GWL_STYLE) | ES_NUMBER);

    // Populate byteUnitComboBox
    HWND byteUnitComboBox = m_Controls[m_IgnoreFilesLargerThanUnitComboBox];
    for (size_t i = 0; i < ARRAYSIZE(kByteUnits); i++)
    {
        SendMessageW(byteUnitComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kByteUnits[i].name));
        SendMessageW(byteUnitComboBox, CB_SETITEMDATA, i, static_cast<LPARAM>(kByteUnits[i].unit));
    }

    // Select MB by default
    SendMessageW(byteUnitComboBox, CB_SETCURSEL, 2 /* MB */, 0);

    // Enable default options
    SendMessageW(m_Controls[m_SearchForFilesCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_SearchInFileNameCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_SearchRecursivelyCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_IgnoreCaseCheckBox], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_Controls[m_IgnoreFilesStartingWithDotCheckBox], BM_SETCHECK, BST_CHECKED, 0);
}

void SearchWindow::AdjustSearchWindowPlacement(int positionX, int positionY, uint32_t dpi)
{
    RECT adjustedWindowRect =
    {
        0,
        0,
        DipsToPixels(kWindowClientWidth, dpi),
        DipsToPixels(kWindowClientHeight, dpi),
    };

    auto result = AdjustWindowRectExForDpi(&adjustedWindowRect, kWindowStyle, FALSE, kWindowExStyle, dpi);
    Assert(result != FALSE);

    result = SetWindowPos(m_Hwnd, nullptr, positionX, positionY, adjustedWindowRect.right - adjustedWindowRect.left, adjustedWindowRect.bottom - adjustedWindowRect.top, SWP_NOACTIVATE | SWP_NOZORDER);
    Assert(result != FALSE);

    auto font = m_FontCache.GetFontForDpi(dpi);
    for (size_t i = 0; i < m_ControlCount; i++)
    {
        HWND childHwnd = m_Controls[i];
        Assert(childHwnd != nullptr);

        const auto& desc = kControls[i];
        auto x = DipsToPixels(desc.x, dpi);
        auto y = DipsToPixels(desc.y, dpi);
        auto width = DipsToPixels(desc.width, dpi);
        auto height = DipsToPixels(desc.height, dpi);

        result = SetWindowPos(childHwnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
        Assert(result != FALSE);

        SendMessageW(childHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void SearchWindow::SearchButtonClicked()
{
    std::wstring searchPath = GetWindowTextW(m_Controls[m_SearchPathTextBox]);
    std::wstring searchPattern = GetWindowTextW(m_Controls[m_SearchPatternTextBox]);
    std::wstring searchString = GetWindowTextW(m_Controls[m_SearchStringTextBox]);

    std::wstring ignoreFilesLargerThan = GetWindowTextW(m_Controls[m_IgnoreFilesLargerThanTextBox]);
    auto comboBoxIndex = SendMessageW(m_Controls[m_IgnoreFilesLargerThanUnitComboBox], CB_GETCURSEL, 0, 0);
    ByteUnit ignoreFilesLargerThanUnit = static_cast<ByteUnit>(SendMessageW(m_Controls[m_IgnoreFilesLargerThanUnitComboBox], CB_GETITEMDATA, comboBoxIndex, 0));
    Assert(ignoreFilesLargerThanUnit <= ByteUnit::TB);

    bool searchForFiles = IsChecked(m_Controls[m_SearchForFilesCheckBox]);
    bool searchInFilePath = IsChecked(m_Controls[m_SearchInFilePathCheckBox]);
    bool searchInFileName = IsChecked(m_Controls[m_SearchInFileNameCheckBox]);
    bool searchInFileContents = IsChecked(m_Controls[m_SearchInFileContentsCheckBox]);
    bool searchContentsAsUtf8 = IsChecked(m_Controls[m_SearchFileContentsAsUTF8CheckBox]);
    bool searchContentsAsUtf16 = IsChecked(m_Controls[m_SearchFileContentsAsUTF16CheckBox]);

    bool searchForDirectories = IsChecked(m_Controls[m_SearchForDirectoriesCheckBox]);
    bool searchInDirectoryPath = IsChecked(m_Controls[m_SearchInDirectoryPathCheckBox]);
    bool searchInDirectoryName = IsChecked(m_Controls[m_SearchInDirectoryNameCheckBox]);

    bool searchRecursively = IsChecked(m_Controls[m_SearchRecursivelyCheckBox]);
    bool ignoreCase = IsChecked(m_Controls[m_IgnoreCaseCheckBox]);
    bool ignoreFilesStartingWithDot = IsChecked(m_Controls[m_IgnoreFilesStartingWithDotCheckBox]);

    bool useDirectStorage = IsChecked(m_Controls[m_UseDirectStorageCheckBox]);

    if (!searchForFiles && !searchForDirectories)
    {
        DisplayValidationFailure(L"Either 'Search for files', and/or 'Search for directories' must be selected.");
        return;
    }

    if (searchForFiles && !searchInFilePath && !searchInFileName && !searchInFileContents)
    {
        DisplayValidationFailure(L"At least one file search mode must be selected if searching for files.");
        return;
    }

    if (searchInFileContents && !searchContentsAsUtf8 && !searchContentsAsUtf16)
    {
        DisplayValidationFailure(L"When searching file contents, UTF8 and/or UTF16 search must be selected.");
        return;
    }

    if (searchForDirectories && !searchInDirectoryPath && !searchInDirectoryName)
    {
        DisplayValidationFailure(L"At least one directory search mode must be selected if searching for directories.");
        return;
    }

    if (searchPath.empty())
    {
        DisplayValidationFailure(L"Search path must not be empty.");
        return;
    }

    if (searchPattern.empty())
    {
        DisplayValidationFailure(L"Search pattern must not be empty.");
        return;
    }

    if (searchString.empty())
    {
        DisplayValidationFailure(L"Search string must not be empty.");
        return;
    }

    if (searchString.length() > 1 << 26)
    {
        if (searchString.length() > (1 << 30) || 
            WideCharToMultiByte(CP_UTF8, 0, searchString.c_str(), static_cast<int>(searchString.length()), nullptr, 0, nullptr, nullptr) > (1 << 30))
        {
            DisplayValidationFailure(L"Search string is too long.");
            return;
        }
    }

    if (searchInFileContents && searchContentsAsUtf8 && 
        std::find_if(searchString.begin(), searchString.end(), [](wchar_t c) { return c < 0 || c > 127; }) != searchString.end())
    {
        DisplayValidationFailure(L"Searching for file contents as UTF8 with non-ascii string is not implemented.");
        return;
    }

    SearchFlags searchFlags = {};

    if (searchForFiles)
        searchFlags |= SearchFlags::kSearchForFiles;

    if (searchInFileName)
        searchFlags |= SearchFlags::kSearchInFileName;

    if (searchInFilePath)
        searchFlags |= SearchFlags::kSearchInFilePath;

    if (searchInFileContents)
        searchFlags |= SearchFlags::kSearchInFileContents;

    if (searchContentsAsUtf8)
        searchFlags |= SearchFlags::kSearchContentsAsUtf8;

    if (searchContentsAsUtf16)
        searchFlags |= SearchFlags::kSearchContentsAsUtf16;

    if (searchForDirectories)
        searchFlags |= SearchFlags::kSearchForDirectories;

    if (searchInDirectoryPath)
        searchFlags |= SearchFlags::kSearchInDirectoryPath;

    if (searchInDirectoryName)
        searchFlags |= SearchFlags::kSearchInDirectoryName;

    if (searchRecursively)
        searchFlags |= SearchFlags::kSearchRecursively;

    if (ignoreCase)
        searchFlags |= SearchFlags::kIgnoreCase;

    if (ignoreFilesStartingWithDot)
        searchFlags |= SearchFlags::kIgnoreDotStart;

    if (useDirectStorage)
        searchFlags |= SearchFlags::kUseDirectStorage;

    uint64_t ignoreLargerThan = 0;
    for (auto c : ignoreFilesLargerThan)
    {
        Assert(c >= '0' && c <= '9');
        if (c < '0' || c > '9' || ignoreLargerThan > std::numeric_limits<uint64_t>::max() / 10)
        {
            ignoreLargerThan = std::numeric_limits<uint64_t>::max();
            break;
        }

        ignoreLargerThan = ignoreLargerThan * 10;
        auto digit = c - '0';

        if (ignoreLargerThan > std::numeric_limits<uint64_t>::max() - digit)
        {
            ignoreLargerThan = std::numeric_limits<uint64_t>::max();
            break;
        }

        ignoreLargerThan += digit;
    }

    if (ignoreLargerThan > std::numeric_limits<uint64_t>::max() / static_cast<uint64_t>(ignoreFilesLargerThanUnit))
    {
        ignoreLargerThan = std::numeric_limits<uint64_t>::max();
    }
    else
    {
        ignoreLargerThan *= static_cast<uint64_t>(ignoreFilesLargerThanUnit);
    }

    auto search = ::Search(
        [](const WIN32_FIND_DATAW* /*findData*/, const wchar_t* path) { std::wstring result = L"Path found: '"; result += path; result += L"'\r\n"; OutputDebugStringW(result.c_str()); },
        [](const SearchStatistics& /*searchStatistics*/, double /*progress*/) {},
        [](const SearchStatistics& /*searchStatistics*/) { OutputDebugStringW(L"Search done!\r\n"); },
        [](const wchar_t* errorMessage) { MessageBoxW(nullptr, errorMessage, L"Error", MB_OK | MB_ICONERROR); },
        searchPath.c_str(),
        searchPattern.c_str(),
        searchString.c_str(),
        searchFlags,
        ignoreLargerThan);

    CleanupSearchOperation(search);
}

void SearchWindow::DisplayValidationFailure(const wchar_t* message)
{
    MessageBoxW(m_Hwnd, message, L"Search is not possible", MB_OK | MB_ICONERROR);
}