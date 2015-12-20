#include "PrecompiledHeader.h"
#include "ExplorerWindow.h"

extern "C" __declspec(dllexport) ExplorerWindow* CreateView(HWND parent, int width, int height)
{
	return new ExplorerWindow(parent, width, height);
}

extern "C" __declspec(dllexport) void InitializeView(ExplorerWindow* window)
{
	window->Initialize();
}

extern "C" __declspec(dllexport) HWND GetHwnd(ExplorerWindow* window)
{
	return window->GetHwnd();
}

extern "C" __declspec(dllexport) void ResizeView(ExplorerWindow* window, int width, int height)
{
	window->ResizeView(width, height);
}

extern "C" __declspec(dllexport) void DestroyView(ExplorerWindow* window)
{
	window->Destroy();
}

extern "C" __declspec(dllexport) void AddItemToView(ExplorerWindow* window, const WIN32_FIND_DATAW* findData, const wchar_t* itemPath)
{
	window->AddItem(findData, itemPath);
}
