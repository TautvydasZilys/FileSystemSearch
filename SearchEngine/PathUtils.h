#pragma once

namespace PathUtils
{

inline std::wstring CombinePaths(std::wstring left, const wchar_t* right)
{
	if (left[left.length() - 1] != L'\\')
		left += L'\\';

	left += right;
	return left;
}

template <typename Allocator>
inline auto CombinePathsTemporary(const std::wstring& left, const wchar_t* right, size_t rightLength, Allocator& allocator, size_t& outLength) -> decltype(allocator.Allocate(0))
{
	auto memory = allocator.Allocate(sizeof(wchar_t) * (left.length() + rightLength + 2));
	wchar_t* ptr = memory;
	
	memcpy(ptr, left.c_str(), left.length() * sizeof(wchar_t));
	ptr += left.length();

	if (left[left.length() - 1] != L'\\')
		*ptr++ = L'\\';

	memcpy(ptr, right, rightLength * sizeof(wchar_t));
	ptr += rightLength;
	*ptr = L'\0';

	outLength = ptr - memory;
	return memory;
}


template <typename Allocator>
inline auto CombinePathsTemporary(const std::wstring& left, const wchar_t* right, size_t rightLength, Allocator& allocator) -> decltype(allocator.Allocate(0))
{
	size_t outLength;
	return CombinePathsTemporary(left, right, rightLength, allocator, outLength);
}

}