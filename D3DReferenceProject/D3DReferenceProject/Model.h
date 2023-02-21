#pragma once

#include "D3DUtils.h"

struct Model
{
	Model() {}
	void Init(const char* path, Vertex3D* vtOutput, uint32_t outputOffset, uint32_t* numVtsLoaded, uint32_t maxVtsPerModel);

	// Need to add CPU-side transforms here
	// (broadcasting every other operation to the GPU is expensive af)
	///////////////////////////////////////
};

