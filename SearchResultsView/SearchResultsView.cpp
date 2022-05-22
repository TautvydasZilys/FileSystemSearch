#include "PrecompiledHeader.h"
#include "SearchResultsView.h"
#include "ExplorerWindow.h"

extern "C" ExplorerWindow* CreateView(HWND parent, int width, int height)
{
	return new ExplorerWindow(parent, width, height, false);
}

extern "C" ExplorerWindow* CreateDpiAwareView(HWND parent, int width, int height)
{
	return new ExplorerWindow(parent, width, height, true);
}

extern "C" void InitializeView(ExplorerWindow* window)
{
	window->Initialize();
}

extern "C" HWND GetHwnd(ExplorerWindow* window)
{
	return window->GetHwnd();
}

extern "C" void ResizeView(ExplorerWindow* window, int width, int height)
{
	window->ResizeView(width, height);
}

extern "C" void DestroyView(ExplorerWindow* window)
{
	window->Destroy();
}

extern "C" void AddItemToView(ExplorerWindow* window, const WIN32_FIND_DATAW* findData, const wchar_t* itemPath)
{
	window->AddItem(findData, itemPath);
}
