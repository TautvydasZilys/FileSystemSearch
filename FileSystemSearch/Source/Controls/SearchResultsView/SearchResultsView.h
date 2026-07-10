#pragma once

class ExplorerWindow;

ExplorerWindow* CreateView(HWND parent, int width, int height);
ExplorerWindow* CreateDpiAwareView(HWND parent, int width, int height);
void InitializeView(ExplorerWindow* window);
HWND GetHwnd(ExplorerWindow* window);
void ResizeView(ExplorerWindow* window, int width, int height);
void DestroyView(ExplorerWindow* window);
void AddItemToView(ExplorerWindow* window, const WIN32_FIND_DATAW* findData, const wchar_t* itemPath);
