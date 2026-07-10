#include "PrecompiledHeader.h"
#include "DirectXContext.h"

static std::once_flag s_OnceFlag;
static ComPtr<IDStorageFactory> s_DStorageFactory;

static void Initialize()
{
    std::call_once(s_OnceFlag, []()
    {
        HRESULT hr;

        hr = DStorageGetFactory(__uuidof(s_DStorageFactory), &s_DStorageFactory);
        Assert(SUCCEEDED(hr));

#if _DEBUG
        if (SUCCEEDED(hr))
            s_DStorageFactory->SetDebugFlags(DSTORAGE_DEBUG_SHOW_ERRORS | DSTORAGE_DEBUG_BREAK_ON_ERROR);
#endif
    });
}

IDStorageFactory* DirectXContext::GetDStorageFactory()
{
    Initialize();
    return s_DStorageFactory.Get();
}
