#pragma once

namespace PathUtils
{

inline std::wstring CombinePaths(std::wstring left, std::wstring_view right)
{
	if (left[left.length() - 1] != L'\\')
	{
		left.reserve(left.length() + right.length() + 1);
		left += L'\\';
	}

	left += right;
	return left;
}

template <typename Allocator>
inline auto CombinePathsTemporary(std::wstring_view left, std::wstring_view right, Allocator& allocator, size_t& outLength) -> decltype(allocator.Allocate(0))
{
	auto memory = allocator.Allocate(sizeof(wchar_t) * (left.length() + right.length() + 2));
	wchar_t* ptr = memory;
	
	memcpy(ptr, left.data(), left.length() * sizeof(wchar_t));
	ptr += left.length();

	if (left[left.length() - 1] != L'\\')
		*ptr++ = L'\\';

	memcpy(ptr, right.data(), right.length() * sizeof(wchar_t));
	ptr += right.length();
	*ptr = L'\0';

	outLength = ptr - memory;
	return memory;
}


template <typename Allocator>
inline auto CombinePathsTemporary(std::wstring_view left, std::wstring_view right, Allocator& allocator) -> decltype(allocator.Allocate(0))
{
	size_t outLength;
	return CombinePathsTemporary(left, right, allocator, outLength);
}

}