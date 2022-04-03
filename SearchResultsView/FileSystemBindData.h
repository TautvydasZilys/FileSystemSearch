#pragma once

class FileSystemBindData : 
	public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IFileSystemBindData2>
{
private:
	static const CLSID CLSID_UnknownJunction;

public:
	FileSystemBindData() : 
		m_ClsidJunction(CLSID_UnknownJunction)
	{
		ZeroMemory(&m_FindData, sizeof(m_FindData));
		ZeroMemory(&m_FileID, sizeof(m_FileID));
	}

	IFACEMETHODIMP SetFindData(const WIN32_FIND_DATAW* findData) override
	{
		m_FindData = findData;
		return S_OK;
	}

	IFACEMETHODIMP GetFindData(WIN32_FIND_DATAW* findData) override
	{
		Assert(m_FindData != nullptr);
		*findData = *m_FindData;
		return S_OK;
	}

	IFACEMETHODIMP SetFileID(LARGE_INTEGER fileID) override
	{
		m_FileID = fileID;
		return S_OK;
	}

	IFACEMETHODIMP GetFileID(LARGE_INTEGER* fileID) override
	{
		*fileID = m_FileID;
		return S_OK;
	}

	IFACEMETHODIMP SetJunctionCLSID(REFCLSID clsid) override
	{
		m_ClsidJunction = clsid;
		return S_OK;
	}

	IFACEMETHODIMP GetJunctionCLSID(CLSID* pclsid) override
	{
		HRESULT hr;
		if (CLSID_UnknownJunction == m_ClsidJunction)
		{
			*pclsid = CLSID_NULL;
			hr = E_FAIL;
		}
		else
		{
			*pclsid = m_ClsidJunction;
			hr = S_OK;
		}
		return hr;
	}

private:
	const WIN32_FIND_DATAW* m_FindData;
	LARGE_INTEGER m_FileID;
	CLSID m_ClsidJunction;
};