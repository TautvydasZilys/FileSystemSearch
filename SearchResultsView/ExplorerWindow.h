#pragma once

#include "HandleHolder.h"

class ExplorerWindow
{
private:
	HWND m_Hwnd;
	WRL::ComPtr<IExplorerBrowser> m_ExplorerBrowser;
	WRL::ComPtr<IResultsFolder> m_ResultsFolder;
	WRL::ComPtr<IBindCtx> m_BindCtx;
	WRL::ComPtr<IFileSystemBindData2> m_FileSystemBindData;
	int m_Width;
	int m_Height;

	~ExplorerWindow();
	static void EnsureWindowClassIsCreated();

	void GetCurrentMonitorScale(float& scaleX, float& scaleY);

public:
	ExplorerWindow(HWND parent, int width, int height);

	void Initialize();
	void Destroy();

	HWND GetHwnd() const { return m_Hwnd; }	
	void ResizeView(int width, int height);
	void AddItem(const WIN32_FIND_DATAW* findData, const wchar_t* path);
};
