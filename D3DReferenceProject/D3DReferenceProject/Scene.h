#pragma once

#include "Model.h"
#include "Camera.h"

class Scene
{
	public:
		Scene();
		void AddModel(const char* path);
		void BakeModels(bool deduplicate); // All models have been submitted, generate scene VB/IB

		void Update();

		void PlayerLook();
		void PlayerMove();

		void GetSceneMesh(D3DHandle* out_vbuffer, D3DHandle* out_ibuffer, uint32_t* out_numIndices); // Needed to pass scene mesh data over to the pipeline for rendering

		static constexpr uint16_t maxNumModels = 256; // Any more than this and storing explicit meshes will be much slower than procedural generation on the GPU

	private:
		Camera playerCamera = {};
		bool cameraMovedSinceLastFrame = false;

		uint16_t currNumModels = 0;
		Model models[maxNumModels] = {};
		bool modelsMovedSinceLastFrame[maxNumModels] = {};

		D3DHandle sceneMeshData_vbuffer = {}; // Beeeg mesh containing all the submeshes associated with this scene
		D3DHandle sceneMeshData_ibuffer = {};

		D3DHandle transforms = {}; // CBuffer with transforms stored in SQT form (scale, quaternion, translation)
								   // Transforms are applied during vertex shading & multiplied against the user's camera
};

