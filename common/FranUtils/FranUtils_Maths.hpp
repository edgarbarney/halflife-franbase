// FranticDreamer 2022-2025

#ifndef FRANUTILS_MATHS_H
#define FRANUTILS_MATHS_H

#include <cstdint>

//#include "FranUtils_Globals.hpp"

namespace FranUtils::Maths
{
	/**
	 * For HL Messages - Returns a long that contains float information
	 *
	 * @see FranUtils::ftol
	 * @param x : Float to store
	 * 
	 * @return Long to send over
	 */
	inline long ftol(float x)
	{
		union
		{
			int32_t i; // long?
			float f;
		} horrible_cast;
		horrible_cast.f = x;
		return horrible_cast.i;
	}

#pragma region Non-ensured Interpolation Funcitons

	/**
	* Basic linear interpolation.
	* Can be used as extrapolation, when lerpfactor is outside of the range [0,1].
	*
	* @see FranUtils::Lerp_s
	* @param lerpfactor : Factor of Interpolation
	* @param A : Starting Point
	* @param B : Ending Point
	* @return Interpolated Value
	*/
	inline float Lerp(float lerpfactor, float A, float B) 
	{ 
		return A + lerpfactor * (B - A); 
	}

	/**
	* (C - A) / (B - A) || Calculation of a float in between 2 floats as a decimal in the range [0,1].
	* Can underflow or overflow. return is not clamped.
	*
	* @remarks Was called "WhereInBetween" in the past.
	* @see FranUtils::Unlerp_s
	* @param find : Number to Find
	* @param min : Starting Point (Minimum)
	* @param max : Ending Point (Maximum)
	* @return Found Value
	*/
	inline float Unlerp(float find, float min, float max) 
	{ 
		return (find - min) / (max - min); 
	}

#pragma endregion

#pragma region Ensured Interpolation Funcitons

	/**
	* Ensured linear interpolation.
	* - Ensure : Can't be used as extrapolation. lerpfactor is clamped the range [0,1].
	*
	* @see FranUtils::Lerp
	* @param lerpfactor : Factor of Interpolation
	* @param A : Starting Point
	* @param B : Ending Point
	* @return Interpolated Value
	*/
	inline float Lerp_s(float lerpfactor, float A, float B)
	{
		if (lerpfactor > 1) 
		{
			return A + 1 * (B - A);
		}
		else if (lerpfactor < 0)
		{ 
			return A + 0 * (B - A);
		}
		else
		{
			return A + lerpfactor * (B - A);
		}
	}

	/**
	* (C - A) / (B - A) || Ensured calculation of a float in between 2 floats as a decimal in the range [0,1].
	* - Ensure : Can't underflow or overflow. return is clamped the range [0,1].
	*
	* @remarks Was called "WhereInBetween_s" in the past.
	* @see FranUtils::Unlerp
	* @param find : Number to Find
	* @param min : Starting Point (Minimum)
	* @param max : Ending Point (Maximum)
	* @return Found Value in the range [0,1]
	*/
	inline float Unlerp_s(float find, float min, float max)
	{
		float calc = (find - min) / (max - min);
		if (calc > 1)
			return 1;
		else if (calc < 0)
			return 0;
		else
			return calc;
	}


#pragma endregion

}

#endif // FRANUTILS_MATHS_H