#pragma once

#include "NonCopyable.h"
#include "Utilities/Control.h"
#include "Utilities/HwndHolder.h"

class ChildControl;
class FontCache;

class SearchWindow : NonCopyable
{
public:
    SearchWindow(const FontCache& fontCache, int nCmdShow);
    ~SearchWindow();

    operator HWND() const
    {
        return m_Hwnd;
    }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void AdjustSearchWindowPlacement(int positionX, int positionY, uint32_t dpi);
    void OnCreate(HWND hWnd);

private:
    #define DECLARE_SEARCH_WINDOW_CONTROLS                                                                                          \
    CONTROL(                                            TextBlock(L"Search Path", 40, 11, 320))                                     \
    NAMED_CONTROL(SearchPathTextBox,                    TextBox(L"C:\\", 40, 30, 320))                                              \
    CONTROL(                                            TextBlock(L"Search Pattern", 40, 71, 320))                                  \
    NAMED_CONTROL(SearchPatternTextBox,                 TextBox(L"*", 40, 90, 320))                                                 \
    CONTROL(                                            TextBlock(L"Search String", 40, 131, 320))                                  \
    NAMED_CONTROL(SearchStringTextBox,                  TextBox(L"", 40, 150, 320))                                                 \
    CONTROL(                                            TextBlock(L"Ignore files larger than", 40, 191, 190))                       \
    NAMED_CONTROL(IgnoreFileLargerThanTextBox,          TextBox(L"10", 40, 210, 190))                                               \
    NAMED_CONTROL(IgnoreFileLargerThanUnitComboBox,     ComboBox(250, 208, 45))                                                     \
                                                                                                                                    \
    NAMED_CONTROL(SearchForFilesCheckBox,               CheckBox(L"Search for files", 41, 240, 320))                                \
    NAMED_CONTROL(SearchInFilePathCheckBox,             CheckBox(L"Search in file path", 41, 260, 320))                             \
    NAMED_CONTROL(SearchInFileNameCheckBox,             CheckBox(L"Search in file name", 41, 280, 320))                             \
    NAMED_CONTROL(SearchInFileContentsCheckBox,         CheckBox(L"Search in file contents", 41, 300, 320))                         \
    NAMED_CONTROL(SearchFileContentsAsUTF8CheckBox,     CheckBox(L"Search file contents as UTF8", 41, 320, 320))                    \
    NAMED_CONTROL(SearchFileContentsAsUTF16CheckBox,    CheckBox(L"Search file contents as UTF16", 41, 340, 320))                   \
                                                                                                                                    \
    NAMED_CONTROL(SearchForDirectoriesCheckBox,         CheckBox(L"Search for directories", 41, 380, 320))                          \
    NAMED_CONTROL(SearchInDirectoryPathCheckBox,        CheckBox(L"Search in directory path", 41, 400, 320))                        \
    NAMED_CONTROL(SearchInDirectoryNameCheckBox,        CheckBox(L"Search in directory name", 41, 420, 320))                        \
                                                                                                                                    \
    NAMED_CONTROL(SearchRecursivelyCheckBox,            CheckBox(L"Search recursively", 41, 460, 320))                              \
    NAMED_CONTROL(IgnoreCaseCheckBox,                   CheckBox(L"Ignore case", 41, 480, 320))                                     \
    NAMED_CONTROL(IgnoreFilesStartingWithDotCheckBox,   CheckBox(L"Ignore files and folders starting with '.'", 41, 500, 320))      \
                                                                                                                                    \
    NAMED_CONTROL(UseDirectStorageCheckBox,             CheckBox(L"Use DirectStorage for reading files", 41, 540, 320))             \
                                                                                                                                    \
    NAMED_CONTROL(SearchButton,                         Button(L"Search!", 40, 580, 320))                                           \

    enum ControlEnum : size_t
    {
#define NAMED_CONTROL(name, control) CONCAT(m_, name),
#define CONTROL(control) CONCAT(m_UnnamedControl, __COUNTER__),

        DECLARE_SEARCH_WINDOW_CONTROLS
        m_ControlCount

#undef CONTROL
#undef NAMED_CONTROL
    };

#define NAMED_CONTROL(name, control) CONTROL(control)
#define CONTROL(control) control,
    static constexpr ControlDescription kControls[m_ControlCount] =
    {
        DECLARE_SEARCH_WINDOW_CONTROLS
    };

#undef CONTROL
#undef NAMED_CONTROL

private:
    const FontCache& m_FontCache;
    ATOM m_WindowClass;
    HWND m_Hwnd;
    HwndHolder m_Controls[m_ControlCount];
};
