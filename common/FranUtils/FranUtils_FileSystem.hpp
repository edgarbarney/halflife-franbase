// FranticDreamer 2022-2025

#ifndef FRANUTILS_FILESYSTEM_H
#define FRANUTILS_FILESYSTEM_H

#include <string>

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

	// Because of Valve's broken include order, we can't just include
	// Filesystem header here. So we should inform the devs
#if defined _FILESYSTEM_ || defined _GLIBCXX_FILESYSTEM

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

#else

#error "You are trying to use FranUtils.FileSystem but you didn't include standard library filesystem header. Please include it in an appropriate place."

#endif // _FILESYSTEM_
}

#endif // FRANUTILS_FILESYSTEM_H