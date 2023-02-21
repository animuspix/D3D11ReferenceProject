#include "Scene.h"
#include "Memory.h"
#include "D3DResource.h"

const uint32_t maxNumVts = 1048576;
Vertex3D* modelVts = {};
//uint32_t* modelNdces = {};
uint32_t numVts = 0;

Scene::Scene()
{
	modelVts = Memory::AllocateArray<Vertex3D>(maxNumVts);
	//modelNdces = Memory::AllocateArray<uint32_t>(maxNumVts);
}

void Scene::AddModel(const char* path)
{
	uint32_t numVtsLoaded = 0;
	models[currNumModels].Init(path, modelVts, numVts, &numVtsLoaded, maxNumVts - numVts);
	numVts += numVtsLoaded;
}

void Scene::BakeModels(bool deduplicate)
{
	// Generate index buffer
	uint32_t* modelNdces = Memory::AllocateArray<uint32_t>(maxNumVts);
	uint32_t numNdces = numVts;

	// Seems likely but not certain that objs are pre-indexed
	// probably not something to assume but good to know ^_^'

	// Two phases needed - load & de-duplicate
	// Naive de-duplication is very slow (On^2 complexity)
	// Progressive model perhaps best - select a vertex, scan the existing index buffer for duplicates, and insert if unique

	// Quad indices
	// 0  1
	// 3  2

	// 0,1,2,2,3,0
	// 0,1,2,3,4,5,6

	// Four in-plane quads
	//
	// Indexed
	//
	// 0 1   4 5
	// 3 2   7 6
	//     .
	// 8 9   C D
	// B A   F E
	//
	// Expected indices
	//
	// 0,1,2,2,3,0 - 4,5,6,6,7,4 - 8,9,A,A,B,8 - C,D,E,E,F,C
	//

//#define LOG_INDICES
//#define INDEXATION_DEBUG_VERTS
#ifdef INDEXATION_DEBUG_VERTS
#define LOG_INDICES

	const uint32_t numTestVts = 24;
	Vertex3D test[numTestVts] = {};

	// First triangle
	test[0].pos = { -2.0f, -2.0f, 0.0f, 0.0f };
	test[1].pos = { -1.0f, -2.0f, 0.0f, 0.0f };
	test[2].pos = { -1.0f, -1.0f, 0.0f, 0.0f };

	// Second triangle
	test[3].pos = test[2].pos;
	test[4].pos = { -2.0f, -1.0f, 0.0f, 0.0f };
	test[5].pos = test[0].pos;

	// Third triangle
	test[6].pos = { 1.0f, -2.0f, 0.0f, 0.0f };
	test[7].pos = { 2.0f, -2.0f, 0.0f, 0.0f };
	test[8].pos = { 2.0f, -1.0f, 0.0f, 0.0f };

	// Fourth triangle
	test[9].pos = test[8].pos;
	test[10].pos = { 1.0f, -1.0f, 0.0f, 0.0f };
	test[11].pos = test[6].pos;

	// Fifth triangle
	test[12].pos = { -2.0f, 1.0f, 0.0f, 0.0f };
	test[13].pos = { -1.0f, 1.0f, 0.0f, 0.0f };
	test[14].pos = { -1.0f, 2.0f, 0.0f, 0.0f };

	// Sixth triangle
	test[15].pos = test[14].pos;
	test[16].pos = { -2.0f, 2.0f, 0.0f, 0.0f };
	test[17].pos = test[12].pos;

	// Seventh triangle
	test[18].pos = { 1.0f, 1.0f, 0.0f, 0.0f };
	test[19].pos = { 2.0f, 1.0f, 0.0f, 0.0f };
	test[20].pos = { 2.0f, 2.0f, 0.0f, 0.0f };

	// Eighth triangle
	test[21].pos = test[20].pos;
	test[22].pos = { 1.0f, 2.0f, 0.0f, 0.0f };
	test[23].pos = test[18].pos;

	// 0 1   4 5
	// 3 2   7 6
	//     .
	// 8 9   C D
	// B A   E F
#endif

#ifdef INDEXATION_DEBUG_VERTS
	numVts = numTestVts;
#endif

	// 24 vertices before indexing, 16 after
	uint32_t uniqueNdxCounter = 0;
	for (uint32_t i = 0; i < numVts; i++)
	{
#ifdef INDEXATION_DEBUG_VERTS
#define vtArray test
#else
#define vtArray modelVts
#endif
		Vertex3D& vt = vtArray[i];

		bool dupFound = false;
		if (deduplicate)
		{
			for (uint32_t j = 0; j < i; j++)
			{
				if (vtArray[j] == vt)
				{
					modelNdces[i] = modelNdces[j];
					dupFound = true;
				}
			}
		}

		if (!dupFound)
		{
			modelNdces[i] = uniqueNdxCounter;
			uniqueNdxCounter++;
		}

#ifdef LOG_INDICES
		// Read in the current index
		const uint8_t ndxTextBufLen = 17;
		char lastNdx[ndxTextBufLen] = {};
		_itoa_s(modelNdces[i], lastNdx, 16); // Hexadecimal encoding, matching comments above

		// Find out how many characters that needed
		uint32_t textBufUsedLen = 0;
		for (uint32_t i = 0; i < ndxTextBufLen; i++)
		{
			if (lastNdx[i] == '\0')
			{
				break;
			}
			else
			{
				textBufUsedLen++;
			}
		}

		// Append comma + space
		char output[ndxTextBufLen + 3] = {}; // Zero-init conveniently flushes with null terminators ^^
		memcpy(output, lastNdx, textBufUsedLen);
		output[textBufUsedLen] = ',';
		output[textBufUsedLen + 1] = ' ';

		// Wrap lines on triangles
		uint8_t terminatorOffs = 2;
		if ((i + 1) % 3 == 0)
		{
			output[textBufUsedLen + terminatorOffs] = '\n';
			terminatorOffs += 1;
		}

		// Append null terminator, print
		OutputDebugStringA(output);
#endif
	}

#ifdef INDEXATION_DEBUG_VERTS
	// Indexation with fake verts invalidates the rest of this block - halt here
	// Technique for messages in assertions found here ^_^
	// https://stackoverflow.com/questions/3692954/add-custom-messages-in-assert
	assert(("Indexation performed with test/placeholder verts; good for debugging, but invalidates model baking", false));
#endif

	D3DResource<RESOURCE_TYPES::BUFFER> ibuffer;
	D3DResource<RESOURCE_TYPES::BUFFER>::D3DResourceDesc ibufDesc;
	ibufDesc.elts_per_axis[0] = numNdces;
	ibufDesc.init_data = modelNdces;
	ibufDesc.data_footprint_bytes = sizeof(uint32_t) * numNdces;
	ibufDesc.fmt = DXGI_FORMAT_R32_UINT;
	ibuffer.Init(ibufDesc, RESRC_ACCESS_TYPES::GPU_ONLY, INDEX);
	sceneMeshData_ibuffer = ibuffer.resource_handle;

	// Reduce [modelVts] to match index buffer
	// (loan a copy of the buffer from our allocator, feed in verts corresponding to values in the index buffer, copy the buffer back over [modelVts], return the loan)
	Vertex3D* tmpVts = Memory::AllocateArray<Vertex3D>(maxNumVts);
	for (uint32_t i = 0; i < numNdces; i++)
	{
		tmpVts[modelNdces[i]] = modelVts[modelNdces[i]];
	}

	// Could zero modelVts here, but expensive and no reason since the excess data won't be used

	// Copy tmpVts back over modelVts
	memcpy(modelVts, tmpVts, sizeof(Vertex3D) * numNdces);
	Memory::FreeToAddress(tmpVts);
	Memory::FreeToAddress(modelNdces);

	// Generate vertex buffer
	D3DResource<RESOURCE_TYPES::BUFFER> vbuffer;
	D3DResource<RESOURCE_TYPES::BUFFER>::D3DResourceDesc vbDesc;
	vbDesc.elts_per_axis[0] = numNdces;
	vbDesc.init_data = modelVts;
	vbDesc.data_footprint_bytes = numNdces * sizeof(Vertex3D);
	vbDesc.fmt = DXGI_FORMAT_UNKNOWN;
	vbuffer.Init(vbDesc, RESRC_ACCESS_TYPES::GPU_ONLY, VERTEX);
	sceneMeshData_vbuffer = vbuffer.resource_handle;
}

void Scene::Update()
{
}

void Scene::PlayerLook()
{
}

void Scene::GetSceneMesh(D3DHandle* out_vbuffer, D3DHandle* out_ibuffer, uint32_t* out_numIndices)
{
	*out_ibuffer = sceneMeshData_ibuffer;
	*out_vbuffer = sceneMeshData_vbuffer;
	*out_numIndices = numVts;
}
