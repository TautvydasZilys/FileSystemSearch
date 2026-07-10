#include "PrecompiledHeader.h"
#include "SearchResultsView.h"
#include "ExplorerWindow.h"

ExplorerWindow* CreateView(HWND parent, int width, int height)
{
	return new ExplorerWindow(parent, width, height, false);
}

ExplorerWindow* CreateDpiAwareView(HWND parent, int width, int height)
{
	return new ExplorerWindow(parent, width, height, true);
}

void InitializeView(ExplorerWindow* window)
{
	window->Initialize();
}

HWND GetHwnd(ExplorerWindow* window)
{
	return window->GetHwnd();
}

void ResizeView(ExplorerWindow* window, int width, int height)
{
	window->ResizeView(width, height);
}

void DestroyView(ExplorerWindow* window)
{
	window->Destroy();
}

void AddItemToView(ExplorerWindow* window, const WIN32_FIND_DATAW* findData, const wchar_t* itemPath)
{
	window->AddItem(findData, itemPath);
}
