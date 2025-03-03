// FranticDreamer 2022-2025

#ifndef FRANUTILS_STRING_H
#define FRANUTILS_STRING_H

#include <string>
#include <vector>
#include <algorithm>

#include "FranUtils_Globals.hpp"

namespace FranUtils::StringUtils
{
	// Returns the first occurrence of a substring in a string
	// Case-insensitive
	// Returns nullptr if the substring is not found
	// e.g. strcharstr("Hello World", "world") returns "World"
	// e.g. strcharstr("Hello World", "worlds") returns nullptr
	inline char* strcharstr(const char* mainstr, const char* substr)
	{
		const char* buffer1 = mainstr;
		const char* buffer2 = substr;
		const char* result = *buffer2 == 0 ? mainstr : 0;

		while (*buffer1 != 0 && *buffer2 != 0)
		{
			if (tolower((unsigned char)*buffer1) == tolower((unsigned char)*buffer2))
			{
				if (result == 0)
				{
					result = buffer1;
				}

				buffer2++;
			}
			else
			{
				buffer2 = substr;
				if (result != 0)
				{
					buffer1 = result + 1;
				}

				if (tolower((unsigned char)*buffer1) == tolower((unsigned char)*buffer2))
				{
					result = buffer1;
					buffer2++;
				}
				else
				{
					result = 0;
				}
			}

			buffer1++;
		}

		return *buffer2 == 0 ? (char*)result : 0;
	}

	// Split quoted tokens into words
	// e.g. 
	// " \"Hello From The Other Side\" " 
	// returns {"Hello From The Other Side"}
	// e.g. 
	// " \"Hello\" \"From\" \"The\" \"Other\" \"Side\" " 
	// returns {"Hello", "From", "The", "Other", "Side"}
	inline std::vector<std::string> SplitQuotedWords(std::string str)
	{
		std::vector<std::string> out;
		std::string tempStr;

		size_t firstQuotePos = 0;
		size_t secondQuotePos = 0;

		do
		{
			firstQuotePos = str.find("\"");
			secondQuotePos = str.find("\"", firstQuotePos + 1);

			if (firstQuotePos == std::string::npos || secondQuotePos == std::string::npos)
			{
				break;
			}
			tempStr = std::string(&str[firstQuotePos], &str[secondQuotePos + 1]);

			// Remove first and last characters of the string, which are the quote marks
			tempStr.pop_back(); tempStr.erase(tempStr.begin());

			out.push_back(tempStr);

			str.erase(firstQuotePos, secondQuotePos + 1);
		} 
		while (firstQuotePos != std::string::npos);
			
		return out;
	}

	inline void LowerCase_Ref(std::string& str)
	{
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char _c)
			{ return std::tolower(_c); });
	}

	inline std::string LowerCase(std::string str)
	{
		LowerCase_Ref(str);
		return str;
	}

	inline void UpperCase_Ref(std::string& str)
	{
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char _c)
			{ return std::toupper(_c); });
	}

	inline std::string UpperCase(std::string str)
	{
		UpperCase_Ref(str);
		return str;
	}
}

#endif // FRANUTILS_STRING_H