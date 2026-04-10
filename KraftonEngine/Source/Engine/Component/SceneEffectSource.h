#pragma once

#include "Render/Pipeline/RenderConstants.h"

// 씬 효과 제공자가 FScene에 노출해야 하는 최소 계약.
class ISceneEffectSource
{
public:
	virtual ~ISceneEffectSource() = default;

	virtual bool IsSceneEffectActive() const = 0;
	virtual void WriteSceneEffectConstants(FSceneEffectConstants& OutConstants, uint32 SlotIndex) const = 0;
};
