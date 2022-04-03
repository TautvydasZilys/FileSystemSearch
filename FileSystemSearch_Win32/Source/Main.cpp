#include "PrecompiledHeader.h"
#include "AppWindows/SearchWindow.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
    SearchWindow searchWindow(nCmdShow);

    for (;;)
    {
        MSG msg;
        auto getMessageResult = GetMessageW(&msg, nullptr, 0, 0);
        Assert(getMessageResult != -1);

        if (getMessageResult == 0 || getMessageResult == -1)
            break;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}