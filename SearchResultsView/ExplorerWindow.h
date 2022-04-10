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
	int m_Width;
	int m_Height;
	bool m_IsDPIAware;

	~ExplorerWindow();
	static void EnsureWindowClassIsCreated();

	void GetCurrentMonitorScale(float& scaleX, float& scaleY);

public:
	ExplorerWindow(HWND parent, int width, int height, bool dpiAware);

	void Initialize();
	void Destroy();

	HWND GetHwnd() const { return m_Hwnd; }	
	void ResizeView(int width, int height);
	void AddItem(const WIN32_FIND_DATAW* findData, const wchar_t* path);
};
