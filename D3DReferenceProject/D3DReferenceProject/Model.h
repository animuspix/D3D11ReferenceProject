#pragma once

#include "D3DUtils.h"

struct Model
{
	Model(const char* path);

	D3DHandle vbuffer = {};
	D3DHandle ibuffer = {};
};

