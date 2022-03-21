#include "PrecompiledHeader.h"
#include "DirectXContext.h"

static std::once_flag s_OnceFlag;
static ComPtr<ID3D12Device> s_D3D12Device;
static ComPtr<IDStorageFactory> s_DStorageFactory;

static void Initialize()
{
    std::call_once(s_OnceFlag, []()
    {
        HRESULT hr;
#if _DEBUG
        constexpr DWORD dxgiFlags = DXGI_CREATE_FACTORY_DEBUG;
#else
        constexpr DWORD dxgiFlags = 0;
#endif

#if _DEBUG
        ComPtr<ID3D12Debug> d3d12Debug;
        hr = D3D12GetDebugInterface(__uuidof(d3d12Debug), &d3d12Debug);
        if (SUCCEEDED(hr))
            d3d12Debug->EnableDebugLayer();
#endif

        ComPtr<IDXGIFactory4> dxgiFactory;
        hr = CreateDXGIFactory2(dxgiFlags, __uuidof(dxgiFactory), &dxgiFactory);

        if constexpr (dxgiFlags & DXGI_CREATE_FACTORY_DEBUG)
        {
            if (FAILED(hr))
                hr = CreateDXGIFactory2(dxgiFlags & ~DXGI_CREATE_FACTORY_DEBUG, __uuidof(dxgiFactory), &dxgiFactory);
        }

        Assert(SUCCEEDED(hr));

        if (SUCCEEDED(hr))
        {
            ComPtr<IUnknown> warpAdapter;
            hr = dxgiFactory->EnumWarpAdapter(__uuidof(warpAdapter), &warpAdapter);
            Assert(SUCCEEDED(hr));

            if (SUCCEEDED(hr))
            {
                hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(s_D3D12Device), &s_D3D12Device);
                Assert(SUCCEEDED(hr));
            }
        }

        hr = DStorageGetFactory(__uuidof(s_DStorageFactory), &s_DStorageFactory);
        Assert(SUCCEEDED(hr));

#if _DEBUG
        if (SUCCEEDED(hr))
            s_DStorageFactory->SetDebugFlags(DSTORAGE_DEBUG_SHOW_ERRORS | DSTORAGE_DEBUG_BREAK_ON_ERROR);
#endif
    });
}

ID3D12Device* DirectXContext::GetD3D12Device()
{
    Initialize();
    return s_D3D12Device.Get();
}

IDStorageFactory* DirectXContext::GetDStorageFactory()
{
    Initialize();
    return s_DStorageFactory.Get();
}
