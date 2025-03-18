// FranticDreamer 2022-2025

#ifndef FRANUTILS_FILESYSTEM_H
#define FRANUTILS_FILESYSTEM_H

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include "FranUtils_Globals.hpp"
#include "FranUtils_String.hpp"

#ifdef CLIENT_DLL
extern cl_enginefunc_t gEngfuncs;
#define GetGameDir    (*gEngfuncs.pfnGetGameDirectory)
#else
extern enginefuncs_t g_engfuncs;
#define GetGameDir    (*g_engfuncs.pfnGetGameDir)
#endif

// Filesystem

namespace FranUtils::FileSystem
{
	// A map that stores key-value pairs as strings
	// This is used as a container for parsed files' output
	using StringMap = std::unordered_map<std::string, std::string>;
	using ComplexStringMap = std::unordered_map<std::string, std::vector<std::string>>;

	inline std::string GetModDirectory(std::string endLine = "//") //Yes, string
	{
		std::string temp = std::filesystem::current_path().string();
		#ifdef CLIENT_DLL
			const char* getGamedir;
			getGamedir = GetGameDir();
		#else
			char getGamedir[120] = "\0";
			GetGameDir(getGamedir);
		#endif
		temp = temp + "//" + getGamedir + endLine;
		return temp;
	}

	// Check if a file exists in mod dir or fallback dir.
	inline bool Exists(const std::string& file)
	{
		if (std::filesystem::exists(GetModDirectory() + file))
			return true; // Mod dir file
		else if (std::filesystem::exists(std::filesystem::current_path().string() + "//" + FranUtils::Globals::GetFallbackDir() + "//" + file))
			return true; // Fallback dir file
		else
			return false;
	}

	// Check if a file exists and open if it does.
	// Returns false if the file doesn't exist
	inline bool OpenInputFile(const std::string& file, std::ifstream& stream)
	{
		// Mod dir file
		if (std::filesystem::exists(GetModDirectory() + file))
		{
			stream.open(GetModDirectory() + file);
			return true;
		}
		// Fallback dir file
		else if (std::filesystem::exists(std::filesystem::current_path().string() + "//" + FranUtils::Globals::GetFallbackDir() + "//" + file))
		{
			stream.open(std::filesystem::current_path().string() + "//" + FranUtils::Globals::GetFallbackDir() + "//" + file);
			return true;
		}

		return false;
	}

	// Splits the file into key-value pairs, separated by a space.
	inline void ParseBasicFileStream(std::ifstream& fstream, StringMap& out)
	{
		std::string line;
		std::string key;
		std::string value;
		while (std::getline(fstream, line))
		{
			key.clear();
			value.clear();

			if (line.empty()) // Ignore empty lines
			{
				continue;
			}
			else if (line[0] == '/') // Ignore comments
			{
				continue;
			}
			// Split line into key and value with a space as a delimiter
			else if (line.find(' ') != std::string::npos)
			{
				key = line.substr(0, line.find(' '));
				value = line.substr(line.find(' ') + 1);
			}
			else
			{
				key = line;
			}

			// Remove quotation marks from the key and value if they exist
			if (key[0] == '\"')
			{
				key = key.substr(1, key.size() - 2);
			}
			if (value[0] == '\"')
			{
				value = value.substr(1, value.size() - 2);
			}

			out[key] = value;
		}
	}

	// Check if a file exists and parse if it does.
	// Returns false if the file doesn't exist
	//
	// Splits the file into key-value pairs, separated by a space.
	inline bool ParseBasicFile(const std::string& file, StringMap& out)
	{
		std::ifstream fstream;
		if (OpenInputFile(file, fstream))
		{
			ParseBasicFileStream(fstream, out);
			fstream.close();
			return true;
		}
		return false;
	}

	// Splits file into lines, separated by spaces.
	inline void ParseSpacedFileStreamLines(std::ifstream& fstream, ComplexStringMap& out)
	{
		std::string line;
		std::string key;
		std::vector<std::string> values;
		while (std::getline(fstream, line))
		{
			key.clear();
			values.clear();

			if (line.empty()) // Ignore empty lines
			{
				continue;
			}
			else if (line[0] == '/') // Ignore comments
			{
				continue;
			}
			// Split line into key and value with first space as a delimiter
			// Then split the value into a vector of strings with spaces as delimiters
			else if (line.find(' ') != std::string::npos)
			{
				key = line.substr(0, line.find(' '));
				auto value = line.substr(line.find(' ') + 1);
				values = FranUtils::StringUtils::SplitWords(value);
			}
			else
			{
				key = line;
			}

			// Remove quotation marks from the key and value if they exist
			if (key[0] == '\"')
			{
				key = key.substr(1, key.size() - 2);
			}
			for (auto& value : values)
			{
				if (value[0] == '\"')
				{
					value = value.substr(1, value.size() - 2);
				}
			}

			out[key] = values;
		}
	}

	// Check if a file exists and parse if it does.
	// Returns false if the file doesn't exist
	//
	// Splits file into lines, separated by spaces.
	inline bool ParseSpacedFile(const std::string& file, ComplexStringMap& out)
	{
		std::ifstream fstream;
		if (OpenInputFile(file, fstream))
		{
			ParseSpacedFileStreamLines(fstream, out);
			fstream.close();
			return true;
		}
		return false;
	}

	// TODO: Rewrite this using ParseBasicFile
	inline void ParseLiblist(std::string& fallbackDir, std::string& startMap, std::string& trainMap, std::string& gameName)
	{
		if (std::filesystem::exists(GetModDirectory() + "liblist.gam"))
		{
			std::ifstream fstream;
			fstream.open(GetModDirectory() + "liblist.gam");

			int lineIteration = 0;
			std::string line;
			while (std::getline(fstream, line))
			{
				lineIteration++;
				// line.erase(remove_if(line.begin(), line.end(), isspace), line.end()); // Remove whitespace
				gEngfuncs.Con_DPrintf("\n liblist.gam - parsing %d", lineIteration);
				if (line.empty()) // Ignore empty lines
				{
					continue;
				}
				else if (line[0] == '/') // Ignore comments
				{
					continue;
				}
				else if (line.substr(0, 13) == "fallback_dir ") // Fallback dir line
				{
					line = line.erase(0, 13);
					auto words = FranUtils::StringUtils::SplitQuotedWords(line);
					std::string& fbdir = words[0];

					if (!fbdir.empty())
					{
						FranUtils::StringUtils::LowerCase_Ref(fbdir);
						fallbackDir = fbdir;
					}
				}
				else if (line.substr(0, 9) == "startmap ") // Startmap line
				{
					line = line.erase(0, 9);
					auto words = FranUtils::StringUtils::SplitQuotedWords(line);
					std::string& stMap = words[0];

					if (!stMap.empty())
					{
						FranUtils::StringUtils::LowerCase_Ref(stMap);
						startMap = stMap;
					}
				}
				else if (line.substr(0, 9) == "trainmap ") // Trainmap line
				{
					line = line.erase(0, 9);
					auto words = FranUtils::StringUtils::SplitQuotedWords(line);
					std::string& trMap = words[0];

					if (!trMap.empty())
					{
						FranUtils::StringUtils::LowerCase_Ref(trMap);
						trainMap = trMap;
					}
				}
				else if (line.substr(0, 5) == "game ") // Game line
				{
					line = line.erase(0, 5);
					auto words = FranUtils::StringUtils::SplitQuotedWords(line);
					std::string& gameStr = words[0];

					if (!gameStr.empty())
					{
						gameName = gameStr;
					}
				}
			}
		}
	}
}

#endif // FRANUTILS_FILESYSTEM_H