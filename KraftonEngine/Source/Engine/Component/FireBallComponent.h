#pragma once

#include "PrimitiveComponent.h"
#include "Math/Rotator.h"

class UFireBallComponent : UPrimitiveComponent
{
	float Intensity;
	float Radius;
	float RadiusFallOff;
	FLinearColor Color;
};

//DECLARE	_CLASS(URotatingMovementComponent, UMovementComponent)