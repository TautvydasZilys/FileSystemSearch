#pragma once

#include <Windows.h>

#undef min
#undef max

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
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