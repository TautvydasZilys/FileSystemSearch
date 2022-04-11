#include "PrecompiledHeader.h"
#include "NumberFormatter.h"

NumberFormatter::NumberFormatter()
{
    RefreshLocaleInfo();
}

void NumberFormatter::RefreshLocaleInfo()
{
    auto resultLength = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, m_DecimalSeparator.data(), static_cast<int>(m_DecimalSeparator.size()));
    Assert(resultLength != 0);

    resultLength = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SNEGATIVESIGN, m_NegativeSign.data(), static_cast<int>(m_NegativeSign.size()));
    Assert(resultLength != 0);

    resultLength = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SPOSITIVESIGN, m_PositiveSign.data(), static_cast<int>(m_PositiveSign.size()));
    Assert(resultLength != 0);

    if (m_PositiveSign[0] == L'\0')
    {
        // From MSDN: If this the data is blank/empty then a "+" is assumed to be used. 
        m_PositiveSign[0] = L'+';
        m_PositiveSign[1] = L'\0';
    }

    DWORD digitSubstitutionMode;
    resultLength = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IDIGITSUBSTITUTION | LOCALE_RETURN_NUMBER, reinterpret_cast<wchar_t*>(&digitSubstitutionMode), sizeof(digitSubstitutionMode));
    Assert(resultLength == sizeof(digitSubstitutionMode) / sizeof(wchar_t));

    bool useDigitSubstitution = false;
    switch (digitSubstitutionMode)
    {
        case 0:
        case 1:
            useDigitSubstitution = false;
            break;

        case 2:
            useDigitSubstitution = true;
            break;
    }

    if (useDigitSubstitution)
    {
        resultLength = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SNATIVEDIGITS, m_Digits.data(), static_cast<int>(m_Digits.size()));
        Assert(resultLength != 0);
    }
    else
    {
        for (wchar_t i = 0; i < 10; i++)
            m_Digits[i] = i + L'0';
    }
}

static size_t GetDigitCount(uint64_t number)
{
    if (number <= 99'999'999)
    {
        // 1 - 8 digits
        if (number <= 9'999)
        {
            // 1 - 4 digits
            if (number <= 99)
                return number <= 9 ? 1 : 2;

            return number <= 999 ? 3 : 4;
        }

        // 5 - 8 digits
        if (number <= 999'999)
            return number <= 99'999 ? 5 : 6;

        return number <= 9'999'999 ? 7 : 8;
    }

    if (number <= 9'999'999'999'999'999)
    {
        // 9 - 16 digits
        if (number <= 999'999'999'999)
        {
            // 9 - 12 digits
            if (number <= 9'999'999'999)
                return number <= 999'999'999 ? 9 : 10;

            return number <= 99'999'999'999 ? 11 : 12;
        }

        // 13 - 16 digits
        if (number <= 99'999'999'999'999)
            return number <= 9'999'999'999'999 ? 13 : 14;

        return number <= 999'999'999'999'999 ? 15 : 16;
    }

    // 17 - 20 digits
    if (number <= 999'999'999'999'999'999)
        return number <= 99'999'999'999'999'999 ? 17 : 18;

    return number <= 9'999'999'999'999'999'999 ? 19 : 20;
}

void NumberFormatter::AppendInteger(std::wstring& dest, uint64_t number) const
{
    if (number == 0)
    {
        dest += m_Digits[0];
        return;
    }

    auto digitCount = GetDigitCount(number);
    auto destSize = dest.size();
    dest.resize(destSize + digitCount, m_Digits[0]);

    wchar_t* destPtr = &dest.back();

    do
    {
        auto digit = number % 10;
        number /= 10;

        *destPtr = m_Digits[digit];
        destPtr--;
    }
    while (number > 0);
}

void NumberFormatter::AppendFloat(std::wstring& dest, double number, int precision) const
{
    char buffer[_CVTBUFSIZE];
    int decimalIndex = 0, signIndex = 0;
    if (number < 10'000'000)
    {
        // Floating point format
        _fcvt_s(buffer, number, precision, &decimalIndex, &signIndex);

        dest.reserve(static_cast<size_t>(decimalIndex) + precision + 3);

        if (decimalIndex <= 0)
        {
            dest += m_Digits[0];
            dest += m_DecimalSeparator.data();

            decimalIndex = -decimalIndex;
            for (int i = 0; i < decimalIndex; i++)
                dest += m_Digits[0];

            for (int i = 0; i < precision && buffer[i] != '\0'; i++)
                dest += GetDigit(buffer[i]);
        }
        else
        {
            for (int i = 0; i < decimalIndex; i++)
                dest += GetDigit(buffer[i]);

            dest += m_DecimalSeparator.data();

            for (int i = 0; i < precision && buffer[i] != '\0'; i++)
                dest += GetDigit(buffer[decimalIndex + i]);
        }
    }
    else
    {
        // Exponent format
        precision++;
        _ecvt_s(buffer, number, precision, &decimalIndex, &signIndex);

        dest += GetDigit(buffer[0]);
        dest += m_DecimalSeparator.data();

        for (int i = 1; i < precision && buffer[i] != '\0'; i++)
            dest += GetDigit(buffer[i]);

        dest += L'e';
        if (decimalIndex < 0)
        {
            dest += m_NegativeSign.data();
            decimalIndex = -decimalIndex;
        }
        else
        {
            dest += m_PositiveSign.data();
        }

        AppendInteger(dest, static_cast<uint32_t>(decimalIndex));
    }
}

wchar_t NumberFormatter::GetDigit(char character) const
{
    uint32_t digit = static_cast<uint32_t>(character - '0');
    Assert(digit <= 9);

    if (digit > 9)
        digit = 0;

    return m_Digits[digit];
}
