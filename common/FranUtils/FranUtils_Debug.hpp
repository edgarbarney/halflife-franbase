// FranticDreamer 2022-2025

#ifndef FRANUTILS_DEBUG_H
#define FRANUTILS_DEBUG_H

#include <string>

namespace FranUtils::Debug
{
#ifndef CLIENT_WEAPONS
	/**
	* Prints 2 strings with a boolean value inbetween.
	* Can be used as extrapolation, when lerpfactor is outside of the range [0,1].
	*
	* @param firstStr : First string. Unformattable. You can use it to store variable name (Ex: "Foo: ")
	* @param boolSelf : Boolean
	* @param afterStr : Last string. Formattable.
	* @param Args: Standard C printing format args
	*/
	inline void PRINT_BOOL(std::string firstStr, bool boolSelf, std::string afterStr = "\n", ...)
	{
		#ifdef _DEBUG
			va_list args;
			char* tst = const_cast<char*>(afterStr.c_str());
			va_start(args, tst);
			//firstStr = firstStr + ": %s" + afterStr;
			firstStr = firstStr + "%s" + afterStr;
			#ifdef CLIENT_DLL
			gEngfuncs.Con_Printf(firstStr.c_str(), boolSelf ? "true" : "false", args);
			#else
			ALERT(at_console, firstStr.c_str(), boolSelf ? "true" : "false", args);
			#endif
		#else
			return;
		#endif
	}

	inline void PRINT(const char* cStr, ...) 
	{
		#ifdef _DEBUG
			va_list args;
			va_start(args, cStr);
			#ifdef CLIENT_DLL
			gEngfuncs.Con_Printf(cStr, args);
			#else
			ALERT(at_console, cStr, args);
			#endif
			va_end(args);
		#else
			return;
		#endif
	}

	inline void PRINT(std::string cStr, ...)
	{
		#ifdef _DEBUG
			va_list args;
			char* tst = const_cast<char*>(cStr.c_str());
			va_start(args, tst);
			#ifdef CLIENT_DLL
			gEngfuncs.Con_Printf(cStr.c_str(), args);
			#else
			ALERT(at_console, cStr.c_str(), args);
			#endif
			va_end(args);
		#else
			return;
		#endif
	}
#endif
}

#endif // FRANUTILS_DEBUG_H