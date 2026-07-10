#pragma once

#include "HandleHolder.h"

class ExplorerWindow
{
private:
	HWND m_Hwnd;
	ComPtr<IExplorerBrowser> m_ExplorerBrowser;
	ComPtr<IResultsFolder> m_ResultsFolder;
	ComPtr<IBindCtx> m_BindCtx;
	ComPtr<IFileSystemBindData2> m_FileSystemBindData;
	DWORD m_ResultDispatcherThreadId;
	int m_Width;
	int m_Height;
	bool m_IsDPIAware;

	static void EnsureWindowClassIsCreated();

	void GetCurrentMonitorScale(float& scaleX, float& scaleY);
	void Initialize();

public:
	ExplorerWindow(HWND parent, int width, int height, bool dpiAware);
	~ExplorerWindow();

	HWND GetHwnd() const { return m_Hwnd; }	
	void ResizeView(int width, int height);
	void AddItem(const WIN32_FIND_DATAW* findData, const wchar_t* path);
};
