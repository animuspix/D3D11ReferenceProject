#pragma once

#include "D3DUtils.h"

struct Camera
{
	SQT_Transform transform;

	void AddRotation(DirectX::XMFLOAT3 axis, float angle)
	{
		// ...
	}

	void AddTranslation(DirectX::XMFLOAT3 pos_delta)
	{
		// ...
	}
};