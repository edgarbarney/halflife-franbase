// FranticDreamer 2022-2025

#ifndef FRANUTILS_GLOBALS_H
#define FRANUTILS_GLOBALS_H

#include <string>

namespace FranUtils
{
	namespace FileSystem
	{
		void ParseLiblist(std::string& fallbackDir, std::string& startMap, std::string& trainMap, std::string& gameName);
	}

	class Globals
	{
	private:
		// Liblist Vars

		inline static std::string fallbackDir; // Fallback dir from
		inline static std::string startMap;	   // Starting Map
		inline static std::string trainMap;	   // Training Room Start Map
		inline static std::string gameName;	   // Game Visible Name Displayed in the Steam and Modlist

	public:
		inline static int isPaused; // Is client paused the game?
		inline static float isPausedLastUpdate;
		inline static bool inMainMenu;		 // Is client in main menu? (Not pause menu)
		inline static bool in3DMainMenu;	 // Is client in 3D main menu?
		inline static bool called3DMainMenu; // Is client called 3D menu already?
		inline static bool lastInMainMenu;

		inline static void InitGlobals()
		{
			isPaused = 1;
			isPausedLastUpdate = 0.0f;
			in3DMainMenu = false;
			inMainMenu = true;
			called3DMainMenu = false;
			lastInMainMenu = false;

			FranUtils::FileSystem::ParseLiblist(fallbackDir, startMap, trainMap, gameName);
		}

		// =======
		// Getters
		// =======

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
				return "Half-Life"; // Replace With FranBase?

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
			//gHUD.m_clImgui.FinishExtension();
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
			//gHUD.m_clImgui.FinishExtension();
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
}

#endif // FRANUTILS_GLOBALS_H