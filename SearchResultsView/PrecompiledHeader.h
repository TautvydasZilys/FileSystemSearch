#pragma once

#define NTDDI_VERSION 0x06010000
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include <ShellScalingApi.h>
#include <Shlobj.h>
#include <ShObjIdl.h>
#include <Windows.h>
#include <wrl.h>

#undef min
#undef max

#include <algorithm>
#include <unordered_map>

namespace WRL
{
	using namespace Microsoft::WRL;
}

#if _DEBUG
#define Assert(x) do { if (!(x)) __debugbreak(); } while (false, false)
#else
#define Assert(x) do { if (false, false) (void)(x); } while (false, false)
#endif