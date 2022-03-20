#pragma once

class UnicodeUtf16StringSearcher
{
private:
	const wchar_t* m_Pattern;
	uint32_t m_PatternLength;

public:
	UnicodeUtf16StringSearcher() :
		m_Pattern(nullptr)
	{
	}

	void Initialize(const wchar_t* pattern, size_t patternLength)
	{
		if (patternLength > std::numeric_limits<uint32_t>::max() / 2)
			__fastfail(1);

		m_Pattern = pattern;
		m_PatternLength = static_cast<uint32_t>(patternLength);
	}

	template <typename TextIterator>
	bool HasSubstring(TextIterator textBegin, TextIterator textEnd) const
	{
		while (textEnd - textBegin >= m_PatternLength)
		{
			auto comparisonResult = CompareStringEx(LOCALE_NAME_SYSTEM_DEFAULT, NORM_IGNORECASE, &*textBegin, static_cast<int>(m_PatternLength), m_Pattern, static_cast<int>(m_PatternLength), nullptr, nullptr, 0);
			if (comparisonResult == CSTR_EQUAL)
				return true;

			textBegin++;
		}

		return false;
	}
};