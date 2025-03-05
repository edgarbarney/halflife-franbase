// FranticDreamer 2022-2025

#ifndef FRANUTILS_MATHS_H
#define FRANUTILS_MATHS_H

// To prevent conflicts with min and max macros in Windows.h and minwindef.h
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846 // GCC math.h
#endif

#include <cstdint>
#include <cmath>
#include <algorithm>

//#include "FranUtils_Globals.hpp"

namespace FranUtils::Maths
{
#pragma region Conversion Functions

	/**
	* For HL Messages - Returns a long that contains float information
	*
	* @param x : Float to store
	* 
	* @return Long to send over
	*/
	inline long ContainFloat(float x)
	{
		union
		{
			int32_t i; // long?
			float f;
		} horrible_cast;
		horrible_cast.f = x;
		return horrible_cast.i;
	}

	/**
	* Converts radians to degrees.
	* 
	* @param rad : Radians
	* 
	* @return Degrees
	*/
	inline double Rad2Deg(double rad)
	{
		return rad * (180.0 / M_PI);
	}
	inline float Rad2Deg(float rad)
	{
		return rad * (180.0f / M_PI);
	}
	
	/**
	* Converts degrees to radians.
	* 
	* @param deg : Degrees
	* 
	* @return Radians
	*/
	inline double Deg2Rad(double deg)
	{
		return deg * (M_PI / 180.0);
	}
	inline float Deg2Rad(float deg)
	{
		return deg * (M_PI / 180.0f);
	}

#pragma endregion

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
	* Fast linear interpolation.
	* - Fast : No precision checks, no extrapolation prevention.
	* - Suggested when working on rendering or other performance-critical tasks.
	* 
	* @see FranUtils::Lerp
	* @param lerpfactor : Factor of Interpolation
	* @param start : Starting Point
	* @param end : Ending Point
	* @return Interpolated Value
	*/
	inline float FastLerp(float lerpfactor, float start, float end)
	{
		// If start and end have opposite signs, ensure precision by distributing frac correctly.
		// This avoids potential floating-point precision issues when interpolating across zero.
		if ((start <= 0 && end >= 0) || (start >= 0 && end <= 0))
		{
			return lerpfactor * end + (1 - lerpfactor) * start;
		}

		// If Lerp Factor is exactly 1, return the end value directly to ensure accuracy.
		if (lerpfactor == 1)
			return end;

		// Compute the standard linear interpolation.
		const float x = start + lerpfactor * (end - start);

		// If Lerp Factor > 1, prevent overshooting beyond 'end' when extrapolating.
		// If Lerp Factor < 0, prevent undershooting beyond 'start'.
		// Uses clamping to ensure monotonic behaviour near lerpfactor = 1.
		return (lerpfactor > 1) == (end > start) ? std::min(end, x) : std::max(end, x);
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

#pragma region Matrix Functions

	/**
	* Generates Euler angles given a left-handed orientation matrix.
	* The columns of the matrix contain the forward, left, and up vectors.
	* 
	* @param matrix : Left-handed orientation matrix.
	* @param angles : Receives right-handed counterclockwise rotations in degrees around Y, Z, and X respectively.
	* @param position : Receives the position of the matrix.
	*/
	void MatrixAngles(const float matrix[3][4], Vector& angles, Vector& position)
	{
		MatrixGetColumn(matrix, 3, position);
		MatrixAngles(matrix, angles);
	}

	/**
	* Generates Euler angles given a left-handed orientation matrix.
	* The columns of the matrix contain the forward, left, and up vectors.
	* 
	* @param matrix : Left-handed orientation matrix.
	* @param angles : Receives right-handed counterclockwise rotations in degrees around Y, Z, and X respectively.
	*/
	void MatrixAngles(const float matrix[3][4], float* angles)
	{
		float forward[3];
		float left[3];
		float up[3];

		//
		// Extract the basis vectors from the matrix. Since we only need the Z
		// component of the up vector, we don't get X and Y.
		//
		forward[0] = matrix[0][0];
		forward[1] = matrix[1][0];
		forward[2] = matrix[2][0];
		left[0] = matrix[0][1];
		left[1] = matrix[1][1];
		left[2] = matrix[2][1];
		up[2] = matrix[2][2];

		float xyDist = sqrtf(forward[0] * forward[0] + forward[1] * forward[1]);

		// enough here to get angles?
		if (xyDist > 0.001f)
		{
			// (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
			angles[1] = Rad2Deg(std::atan2f(forward[1], forward[0]));

			// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
			angles[0] = Rad2Deg(std::atan2f(-forward[2], xyDist));

			// (roll)	z = ATAN( left.z, up.z );
			angles[2] = Rad2Deg(std::atan2f(left[2], up[2]));
		}
		else // forward is mostly Z, gimbal lock-
		{
			// (yaw)	y = ATAN( -left.x, left.y );			-- forward is mostly z, so use right for yaw
			angles[1] = Rad2Deg(std::atan2f(-left[0], left[1]));

			// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
			angles[0] = Rad2Deg(std::atan2f(-forward[2], xyDist));

			// Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
			angles[2] = 0;
		}
	}

	void MatrixGetColumn(const float in[3][4], int column, Vector& out)
	{
		out.x = in[0][column];
		out.y = in[1][column];
		out.z = in[2][column];
	}

	void MatrixSetColumn(const Vector& in, int column, float out[3][4])
	{
		out[0][column] = in.x;
		out[1][column] = in.y;
		out[2][column] = in.z;
	}

#pragma endregion

}

#endif // FRANUTILS_MATHS_H