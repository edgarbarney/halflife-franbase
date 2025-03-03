// FranticDreamer 2022-2025

#ifndef FRANUTILS_FILESYSTEM_H
#define FRANUTILS_FILESYSTEM_H

#include <filesystem>
#include <fstream>
#include <string>

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

	inline static void ParseLiblist(std::string& fallbackDir, std::string& startMap, std::string& trainMap, std::string& gameName)
	{
		// Because of Valve's broken include order, we can't just include
		// Filesystem header here. So we should inform the devs
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