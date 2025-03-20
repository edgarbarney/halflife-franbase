// FranticDreamer 2022-2025

#ifndef FRANUTILS_STRING_H
#define FRANUTILS_STRING_H

#include <string>
#include <vector>
#include <algorithm>

#include "FranUtils_Globals.hpp"

namespace FranUtils::StringUtils
{
	// Check if a string contains a substring
	// Case insensitive
	// e.g. HasSubstring("Hello World", "world") returns true
	// e.g. HasSubstring("Hello World", "WORLD") returns true
	// e.g. HasSubstring("Hello World", "Worler") returns false
	inline bool HasInsensitiveSubstring(const std::string& container, const std::string& substring)
	{
		if (substring.empty())
		{
			return true;
		}

		container.find(LowerCase(substring)) != std::string::npos;
	}

	// Check if a string contains a substring
	// Case sensitive
	// e.g. HasSubstring("Hello World", "World") returns true
	// e.g. HasSubstring("Hello World", "WORLD") returns false
	// e.g. HasSubstring("Hello World", "Worler") returns false
	inline bool HasSubstring(const std::string& container, const std::string& substring)
	{
		if (substring.empty())
		{
			return true;
		}

		return container.find(substring) != std::string::npos;
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

	// Split a string into words
	// e.g. SplitWords("Hello World") returns {"Hello", "World"}
	inline std::vector<std::string> SplitWords(std::string str)
	{
		std::vector<std::string> out;
		std::string tempStr;

		size_t spacePos = 0;
		
		do
		{
			spacePos = str.find(" ");

			if (spacePos == std::string::npos)
			{
				spacePos = str.find("\t"); // Check for tabs, too.

				if (spacePos == std::string::npos)
				{
					break;
				}
			}
			tempStr = str.substr(0, spacePos);
			out.push_back(tempStr);

			str.erase(0, spacePos + 1);
		} 
		while (spacePos != std::string::npos);

		out.push_back(str);

		// Remove any whitespace from the words
		std::erase_if(out, [](auto&& str)
		{ 
			return str.empty();
		});

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