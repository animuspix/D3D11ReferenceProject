#pragma once

#include "Model.h"
#include "Camera.h"

class Scene
{
	//public:
	//	Scene(uint16_t numExpectedModels);
	//	void AddModel(const char* path);
	//	void Update();
	//
	//	void PlayerLook();
	//	void PlayerMove();

	private:
		Camera playerCamera = {};
		bool cameraMovedSinceLastFrame = false;

		Model* models = {};
		bool* modelsMovedSinceLastFrame = {};

		D3DHandle transforms = {}; // CBuffer with transforms stored in SQT form (scale, quaternion, translation)
								  // Transforms are applied during vertex shading & multiplied against the user's camera
};

