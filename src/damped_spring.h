
#ifndef DAMPED_SPRING_H
#define DAMPED_SPRING_H

/******************************************************************************
  Copyright (c) 2008-2012 Ryan Juckett
  http://www.ryanjuckett.com/
 
  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.
 
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:
 
  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
 
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
 
  3. This notice may not be removed or altered from any source
     distribution.
******************************************************************************/
//******************************************************************************
// Cached set of motion parameters that can be used to efficiently update
// multiple springs using the same time step, angular frequency and damping
// ratio.
//******************************************************************************
typedef struct {
	// newPos = posPosCoef*oldPos + posVelCoef*oldVel
	float m_posPosCoef, m_posVelCoef;
	// newVel = velPosCoef*oldPos + velVelCoef*oldVel
	float m_velPosCoef, m_velVelCoef;
} tDampedSpringMotionParams;

tDampedSpringMotionParams CalcDampedSpringMotionParams(
	float	                   deltaTime,        // time step to advance
	float	                   angularFrequency, // angular frequency of motion
	float	                   dampingRatio);     // damping ratio of motion

void UpdateDampedSpringMotion(
	float*                           pPos,           // position value to update
	float*                           pVel,           // velocity value to update
	const float                      equilibriumPos, // position to approach
	const tDampedSpringMotionParams  params);         // motion parameters to use
#endif