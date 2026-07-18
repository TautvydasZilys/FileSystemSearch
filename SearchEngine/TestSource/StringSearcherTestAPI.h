#pragma once

#if INCLUDE_TESTS

struct TestStringSearcher;

extern "C" EXPORT_SEARCHENGINE TestStringSearcher* CreateStringSearcher(const wchar_t* searchString, SearchFlags searchFlags);
extern "C" EXPORT_SEARCHENGINE bool SearchFileContents(TestStringSearcher* stringSearcher, uint8_t* fileBytes, uint32_t byteCount);
extern "C" EXPORT_SEARCHENGINE void FreeStringSearcher(TestStringSearcher* stringSearcher);

#endif