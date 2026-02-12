#pragma once

class NumberFormatter
{
public:
    NumberFormatter();
    void RefreshLocaleInfo();

    void AppendInteger(std::wstring& dest, uint64_t number) const;
    void AppendFloat(std::wstring& dest, double number, int precision) const;

private:
    wchar_t GetDigit(char character) const;

private:
    std::array<wchar_t, 4> m_DecimalSeparator;
    std::array<wchar_t, 5> m_NegativeSign;
    std::array<wchar_t, 5> m_PositiveSign;
    std::array<wchar_t, 11> m_Digits;
};