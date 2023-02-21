#pragma once

#include "D3DUtils.h"
#include "Scene.h"

class Pipeline
{
	public:
		static void Init(Scene* scenes, uint8_t numScenes);
		static void DeInit();
		static void PushFrame(uint32_t sceneID);
};

