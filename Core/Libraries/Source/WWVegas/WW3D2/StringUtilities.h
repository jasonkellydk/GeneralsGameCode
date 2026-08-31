#pragma once

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace WW3DString
{
inline int Compare_No_Case(const char *left, const char *right)
{
	if (left == right)
	{
		return 0;
	}

	if (left == nullptr)
	{
		return -1;
	}

	if (right == nullptr)
	{
		return 1;
	}

	while (*left != '\0' && *right != '\0')
	{
		const int left_character = std::tolower(static_cast<unsigned char>(*left));
		const int right_character = std::tolower(static_cast<unsigned char>(*right));
		if (left_character != right_character)
		{
			return left_character - right_character;
		}

		++left;
		++right;
	}

	return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

inline int Compare_No_Case_N(const char *left, const char *right, std::size_t count)
{
	if (count == 0 || left == right)
	{
		return 0;
	}

	if (left == nullptr)
	{
		return -1;
	}

	if (right == nullptr)
	{
		return 1;
	}

	for (std::size_t index = 0; index < count; ++index)
	{
		const unsigned char left_value = static_cast<unsigned char>(left[index]);
		const unsigned char right_value = static_cast<unsigned char>(right[index]);
		const int left_character = std::tolower(left_value);
		const int right_character = std::tolower(right_value);
		if (left_character != right_character)
		{
			return left_character - right_character;
		}

		if (left_value == '\0' || right_value == '\0')
		{
			return left_value - right_value;
		}
	}

	return 0;
}

inline void To_Lower(char *value)
{
	if (value == nullptr)
	{
		return;
	}

	for (; *value != '\0'; ++value)
	{
		*value = static_cast<char>(std::tolower(static_cast<unsigned char>(*value)));
	}
}

inline void To_Upper(char *value)
{
	if (value == nullptr)
	{
		return;
	}

	for (; *value != '\0'; ++value)
	{
		*value = static_cast<char>(std::toupper(static_cast<unsigned char>(*value)));
	}
}

inline char *Duplicate(const char *value)
{
	if (value == nullptr)
	{
		return nullptr;
	}

	const std::size_t length = std::strlen(value);
	char *copy = static_cast<char *>(std::malloc(length + 1));
	if (copy != nullptr)
	{
		std::memcpy(copy, value, length + 1);
	}

	return copy;
}
}
