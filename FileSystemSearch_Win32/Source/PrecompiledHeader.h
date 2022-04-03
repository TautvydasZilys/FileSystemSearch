#pragma once

#define NTDDI_VERSION 0x0A000007 // NTDDI_WIN10_19H1
#define _WIN32_WINNT 0x0A00 // _WIN32_WINNT_WIN10
#define WINVER 0x0A00

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <commctrl.h>

#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

#if _DEBUG
#define Assert(x) do { if (!(x)) __debugbreak(); } while (false, false)
#else
#define Assert(x) do { if (false, false) (void)(x); } while (false, false)
#endif