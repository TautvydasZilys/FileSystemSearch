#include "PrecompiledHeader.h"
#include "StringUtils.h"
#include "TestLogger.h"
#include "TestMacros.h"

void Testing::PrintToStdout(std::string_view text)
{
    CHECK(text.length() <= std::numeric_limits<DWORD>::max(), L"Text is too long to write to stdout!");

    auto stdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD bytesWritten;
    auto writeResult = WriteFile(stdOut, text.data(), static_cast<DWORD>(text.length()), &bytesWritten, nullptr);
    CHECK(writeResult && bytesWritten == text.length(), L"Failed to write to stdout");

    FlushFileBuffers(stdOut);
}

void Testing::PrintToStdout(std::wstring_view wideText)
{
    PrintToStdout(StringUtils::Utf16ToUtf8(wideText));
}