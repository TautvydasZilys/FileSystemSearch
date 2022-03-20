#pragma once

#define NTDDI_VERSION 0x0A000007 // NTDDI_WIN10_19H1
#define _WIN32_WINNT 0x0A00 // _WIN32_WINNT_WIN10
#define WINVER 0x0A00

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>

#include <dstorage.h>
#include <dstorageerr.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <threadpoolapiset.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#define MAKE_BIT_OPERATORS_FOR_ENUM_CLASS(T) \
	inline T operator|(T left, T right) \
	{ \
		typedef std::underlying_type<T>::type UnderlyingType; \
		return static_cast<T>(static_cast<UnderlyingType>(left) | static_cast<UnderlyingType>(right)); \
	} \
	inline T operator&(T left, T right) \
	{ \
		typedef std::underlying_type<T>::type UnderlyingType; \
		return static_cast<T>(static_cast<UnderlyingType>(left) & static_cast<UnderlyingType>(right)); \
	} \
	inline void operator|=(T& left, T right) \
	{ \
		left = left | right; \
	} \
	inline void operator&=(T& left, T right) \
	{ \
		left = left & right; \
	}

#if _DEBUG
#define Assert(x) do { if (!(x)) __debugbreak(); } while (false, false)
#else
#define Assert(x) do { if (false, false) (void)(x); } while (false, false)
#endif