#pragma once

class ExplorerWindow;

extern "C" EXPORT_SEARCHRESULTSVIEW ExplorerWindow* CreateView(HWND parent, int width, int height);
extern "C" EXPORT_SEARCHRESULTSVIEW ExplorerWindow* CreateDpiAwareView(HWND parent, int width, int height);
extern "C" EXPORT_SEARCHRESULTSVIEW void InitializeView(ExplorerWindow* window);
extern "C" EXPORT_SEARCHRESULTSVIEW HWND GetHwnd(ExplorerWindow* window);
extern "C" EXPORT_SEARCHRESULTSVIEW void ResizeView(ExplorerWindow* window, int width, int height);
extern "C" EXPORT_SEARCHRESULTSVIEW void DestroyView(ExplorerWindow* window);
extern "C" EXPORT_SEARCHRESULTSVIEW void AddItemToView(ExplorerWindow* window, const WIN32_FIND_DATAW* findData, const wchar_t* itemPath);
