#include "PrecompiledHeader.h"
#include "AppWindows/SearchWindow.h"
#include "AppWindows/SearchResultWindow.h"
#include "Utilities/FontCache.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
    INITCOMMONCONTROLSEX initCommonControls;
    initCommonControls.dwSize = sizeof(initCommonControls);
    initCommonControls.dwICC = ICC_STANDARD_CLASSES;

    auto result = InitCommonControlsEx(&initCommonControls);
    Assert(result != FALSE);

    FontCache fontCache;
    SearchResultWindow::StaticInitializer searchResultWindowStatics;

    {
        SearchWindow searchWindow(fontCache, nCmdShow);

        for (;;)
        {
            MSG msg;
            auto getMessageResult = GetMessageW(&msg, nullptr, 0, 0);
            Assert(getMessageResult != -1);

            if (getMessageResult == 0 || getMessageResult == -1)
                break;

            if (!IsDialogMessageW(searchWindow, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    return 0;
}