// FranticDreamer 2022-2025

#ifndef FRANUTILS_GLOBALS_H
#define FRANUTILS_GLOBALS_H

#include <string>

class Globals
{
private:
	// Liblist Vars

	inline static std::string fallbackDir; // Fallback dir from
	inline static std::string startMap;	   // Starting Map
	inline static std::string trainMap;	   // Training Room Start Map
	inline static std::string gameName;	   // Game Visible Name Displayed in the Steam and Modlist

	inline static void ParseLiblist()
	{
		// Because of Valve's broken include order, we can't just include
		// Filesystem header here. So we should inform the devs
#if defined _FILESYSTEM_ || defined _GLIBCXX_FILESYSTEM
		if (std::filesystem::exists(FranUtils::GetModDirectory() + "liblist.gam"))
		{
			std::ifstream fstream;
			fstream.open(FranUtils::GetModDirectory() + "liblist.gam");

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
					auto words = SplitQuotedWords(line);
					std::string& fbdir = words[0];

					if (!fbdir.empty())
					{
						LowerCase_Ref(fbdir);
						fallbackDir = fbdir;
					}
				}
				else if (line.substr(0, 9) == "startmap ") // Startmap line
				{
					line = line.erase(0, 9);
					auto words = SplitQuotedWords(line);
					std::string& stMap = words[0];

					if (!stMap.empty())
					{
						LowerCase_Ref(stMap);
						startMap = stMap;
					}
				}
				else if (line.substr(0, 9) == "trainmap ") // Trainmap line
				{
					line = line.erase(0, 9);
					auto words = SplitQuotedWords(line);
					std::string& trMap = words[0];

					if (!trMap.empty())
					{
						LowerCase_Ref(trMap);
						trainMap = trMap;
					}
				}
				else if (line.substr(0, 5) == "game ") // Game line
				{
					line = line.erase(0, 5);
					auto words = SplitQuotedWords(line);
					std::string& gameStr = words[0];

					if (!gameStr.empty())
					{
						gameName = gameStr;
					}
				}
			}
		}
#else

#error "You are trying to use FranUtils.Globals but you didn't include standard library filesystem header. Please include it in an appropriate place."

#endif // _FILESYSTEM_
	}

public:
	inline static int isPaused; // Is client paused the game?
	inline static float isPausedLastUpdate;
	inline static bool inMainMenu;		 // Is client in main menu? (Not pause menu)
	inline static bool in3DMainMenu;	 // Is client in 3D main menu?
	inline static bool called3DMainMenu; // Is client called 3D menu already?
	inline static bool lastInMainMenu;

	inline static void InitGlobals()
	{
		isPaused = true;
		isPausedLastUpdate = 0.0f;
		in3DMainMenu = false;
		inMainMenu = true;
		called3DMainMenu = false;
		lastInMainMenu = false;

		ParseLiblist();
	}

	inline static std::string GetFallbackDir()
	{
		if (fallbackDir.empty())
			return "valve";

		return fallbackDir;
	}

	inline static std::string GetStartMap()
	{
		if (startMap.empty())
			return "c1a0";

		return startMap;
	}

	inline static std::string GetTrainingStartMap()
	{
		if (trainMap.empty())
			return "t1a0";

		return trainMap;
	}

	inline static std::string GetGameDisplayName()
	{
		if (gameName.empty())
			return "Half-Life"; // Replace With Spirinity?

		return gameName;
	}


#if defined(CLIENT_DLL) && defined(CL_ENGFUNCS_DEF)
	inline static void PauseMenu()
	{
		if (Globals::isPaused)
			gEngfuncs.pfnClientCmd("escape");

		// gEngfuncs.pfnClientCmd("toggleconsole");
	}
	inline static void QuitGame()
	{
		gHUD.m_clImgui.FinishExtension();
		gEngfuncs.pfnClientCmd("quit");
	}
	// USE AS A LAST RESORT
	inline static void ForceShutdown()
	{
		// DODGE THIS, ENGINE!!
		// PostQuitMessage(0);
	}
	inline static void RestartGame()
	{
		gHUD.m_clImgui.FinishExtension();
		gEngfuncs.pfnClientCmd("_restart");
	}
#else
	inline static void PauseMenu() {}
	inline static void QuitGame() {}
	// USE AS A LAST RESORT
	inline static void ForceShutdown() {}
	inline static void RestartGame() {}
#endif
};
	

#endif // FRANUTILS_GLOBALS_H