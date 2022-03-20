#pragma once

template <typename CharType>
class OrdinalStringSearcher
{
private:
	static_assert(sizeof(CharType) <= 2, "Character types larger than 2 bytes are not supported");
	typedef typename std::make_unsigned<CharType>::type UnsignedCharType;
	static const size_t kAlphabetSize = 1 << (8 * sizeof(CharType));

	const CharType* m_Pattern;
	std::unique_ptr<uint32_t[]> m_NextSuffixOffset;
	uint32_t m_PatternLength;
	uint32_t m_LastCharacterOccurenceMap[kAlphabetSize];

	inline void PrecomputeMaps()
	{
		for (auto& c : m_LastCharacterOccurenceMap)
			c = m_PatternLength;

		for (uint32_t i = 0; i < m_PatternLength - 1u; i++)
			m_LastCharacterOccurenceMap[static_cast<UnsignedCharType>(m_Pattern[i])] = m_PatternLength - i - 1u;

		m_NextSuffixOffset = std::unique_ptr<uint32_t[]>(new uint32_t[m_PatternLength]);
		m_NextSuffixOffset[m_PatternLength - 1] = 1;

		uint32_t lastPrefixIndex = m_PatternLength - 1u;

		for (uint32_t i = m_PatternLength; i > 0; i--)
		{
			if (memcmp(m_Pattern, m_Pattern + i, sizeof(CharType) * (m_PatternLength - i)) == 0)
				lastPrefixIndex = i;

			m_NextSuffixOffset[i - 1] = static_cast<uint32_t>(lastPrefixIndex + m_PatternLength - i);
		}

		for (uint32_t i = 0; i < m_PatternLength - 1u; i++)
		{
			uint32_t suffixLength = 0;

			while (m_Pattern[i - suffixLength] == m_Pattern[m_PatternLength - suffixLength - 1] && suffixLength < i)
				suffixLength++;

			if (m_Pattern[i - suffixLength] != m_Pattern[m_PatternLength - suffixLength - 1])
				m_NextSuffixOffset[m_PatternLength - suffixLength - 1] = static_cast<uint32_t>(m_PatternLength - 1 - i + suffixLength);
		}
	}

public:
	OrdinalStringSearcher() :
		m_Pattern(nullptr)
	{
	}

	void Initialize(const CharType* pattern, size_t patternLength)
	{
		if (patternLength > std::numeric_limits<uint32_t>::max() / 2)
			__fastfail(1);

		m_Pattern = pattern;
		m_PatternLength = static_cast<uint32_t>(patternLength);

		PrecomputeMaps();
	}

	template <typename TextIterator>
	bool HasSubstring(TextIterator textBegin, TextIterator textEnd) const
	{
		for (;;)
		{
			if (textEnd - textBegin < m_PatternLength)
				return false;

			uint32_t i = m_PatternLength - 1;
			while (i != 0 && textBegin[i] == m_Pattern[i])
				i--;

			if (textBegin[i] == m_Pattern[i])
				return true;

			uint32_t shiftAmount = std::max(m_LastCharacterOccurenceMap[textBegin[i]], m_NextSuffixOffset[i]);
			shiftAmount -= m_PatternLength - i - 1;
			if (textEnd - textBegin < static_cast<int32_t>(shiftAmount))
				return false;

			textBegin += shiftAmount;
		}
	}
};