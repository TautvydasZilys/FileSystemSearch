#pragma once

namespace StringUtils
{

inline std::wstring ToLowerUnicode(const wchar_t* str, size_t length)
{
	auto lowerCaseLength = LCMapStringEx(LOCALE_NAME_SYSTEM_DEFAULT, LCMAP_LOWERCASE, str, static_cast<int>(length), nullptr, 0, nullptr, nullptr, 0);
	if (lowerCaseLength == 0)
		__fastfail(1);

	std::wstring strLowerCase;
	strLowerCase.resize(lowerCaseLength);

	lowerCaseLength = LCMapStringEx(LOCALE_NAME_SYSTEM_DEFAULT, LCMAP_LOWERCASE, str, static_cast<int>(length), &strLowerCase[0], lowerCaseLength, nullptr, nullptr, 0);
	Assert(lowerCaseLength != 0);

	strLowerCase[lowerCaseLength] = L'\0';
	return strLowerCase;
}

inline std::wstring ToLowerUnicode(const wchar_t* str)
{
	return ToLowerUnicode(str, wcslen(str));
}

inline std::wstring ToLowerUnicode(const std::wstring& str)
{
	return ToLowerUnicode(str.c_str(), str.length());
}

template <typename CharType>
inline void ToLowerAscii(const CharType* source, CharType* destination, size_t length)
{
	auto end = source + length;
	for (auto ptr = source; ptr != end; ptr++, destination++)
	{
		if (*ptr >= 'A' && *ptr <= 'Z')
		{
			*destination = *ptr - 'A' + 'a';
		}
		else
		{
			*destination = *ptr;
		}
	}
}

inline void ToLowerAsciiInline(std::wstring& str)
{
	ToLowerAscii(str.c_str(), &str[0], str.length());
}

inline std::string Utf16ToUtf8(std::wstring_view utf16)
{
	if (utf16.empty())
		return std::string();

	auto utf8Length = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.length()), nullptr, 0, nullptr, nullptr);
	if (utf8Length == 0)
		__fastfail(0);

	std::string utf8;
	utf8.resize(utf8Length);
	utf8Length = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.length()), &utf8[0], utf8Length, nullptr, nullptr);
	Assert(utf8Length != 0);

	return utf8;
}

inline std::wstring Utf8ToUtf16(std::string_view utf8)
{
	if (utf8.empty())
		return std::wstring();

	auto utf16Length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.length()), nullptr, 0);
	if (utf16Length == 0)
		__fastfail(0);

	std::wstring utf16;
	utf16.resize(utf16Length);
	utf16Length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.length()), &utf16[0], utf16Length);
	Assert(utf16Length != 0);

	return utf16;
}

inline bool IsAscii(const std::wstring& utf16)
{
	for (const auto& c : utf16)
	{
		if (c > 127)
			return false;
	}

	return true;
}

}