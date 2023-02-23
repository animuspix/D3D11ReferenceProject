#include "Pipeline.h"
#include "D3DWrapper.h"
#include "D3DResource.h"
#include "Memory.h"

// Arrays of resources &c consumed by the pipeline here...
//////////////////////////////////////////////////////////

// Heterogeneous array of jobs consumed by the pipeline

struct ShadingJob
{
	static constexpr uint32_t maxBindingsAnyType = 16;

	void AddTexture(D3DHandle resrc, RESRC_VIEWS bindAs, SHADER_TYPES bindFor)
	{
		if ((bindAs & DEPTH_STENCIL) && hasDepthStencil)
		{
			assert(("No more than one depth-stencil buffer in each draw", false));
		}

		bindTexturesFor[numTextures] = bindFor;
		textureBindings[numTextures] = bindAs;
		textures[numTextures] = resrc;
		numTextures++;
	}

	void AddBuffer(D3DHandle resrc, RESRC_VIEWS bindAs, SHADER_TYPES bindFor)
	{
		bindBufferFor[numBuffers] = bindFor;
		bufferBindings[numBuffers] = bindAs;
		buffers[numBuffers] = resrc;
		numBuffers++;
	}

	void AddVolume(D3DHandle resrc, RESRC_VIEWS bindAs, SHADER_TYPES bindFor)
	{
		bindVolumeFor[numVolumes] = bindFor;
		volumeBindings[numVolumes] = bindAs;
		volumes[numVolumes] = resrc;
		numVolumes++;
	}

	SHADER_TYPES bindTexturesFor[maxBindingsAnyType];
	RESRC_VIEWS textureBindings[maxBindingsAnyType];
	D3DHandle textures[maxBindingsAnyType];
	uint32_t numTextures = 0;

	SHADER_TYPES bindBufferFor[maxBindingsAnyType];
	RESRC_VIEWS bufferBindings[maxBindingsAnyType];
	D3DHandle buffers[maxBindingsAnyType];
	uint32_t numBuffers = 0;

	SHADER_TYPES bindVolumeFor[maxBindingsAnyType];
	RESRC_VIEWS volumeBindings[maxBindingsAnyType];
	D3DHandle volumes[maxBindingsAnyType];
	uint32_t numVolumes = 0;

	bool hasDepthStencil = false;
};

struct DrawJob : public ShadingJob
{
	DrawJob() {}
	DrawJob(const char* vs_path, const char* ps_path) : ShadingJob()
	{
		vs = D3DWrapper::CreateVertShader(vs_path, false);
		ps = D3DWrapper::CreatePixelShader(ps_path);
	}

	bool is2D = false;
	bool directToBackbuf = true; // Set if this draw writes to the back-buffer instead of an intermediate RTV

	D3DHandle vs;
	D3DHandle ps;
};

struct DispatchJob : ShadingJob
{
	DispatchJob() {}
	DispatchJob(const char* cs_path, uint32_t _dispatchX, uint32_t _dispatchY, uint32_t _dispatchZ) : dispatchX(_dispatchX), dispatchY(_dispatchY), dispatchZ(_dispatchZ)
	{
		cs = D3DWrapper::CreateComputeShader(cs_path);
	}

	D3DHandle cs;
	uint32_t dispatchX = 1, dispatchY = 1, dispatchZ = 1;
};

struct CopyJob
{
	// ...
};

struct SceneMesh
{
	D3DHandle vbuffer;
	D3DHandle ibuffer;
	uint32_t numIndices = 0;
};

SceneMesh* sceneData = nullptr;
uint8_t numScenesAvailable = 0;

struct JobArray
{
	static constexpr uint32_t maxDrawJobs = 8;
	static constexpr uint32_t maxComputeJobs = 8;

	enum JOB_TYPES
	{
		DRAW,
		DISPATCH
	};

	DrawJob drawJobsCompact[maxDrawJobs] = {};
	DispatchJob dispatchJobsCompact[maxComputeJobs] = {};
	JOB_TYPES typesOfJob[maxDrawJobs + maxComputeJobs] = {};
	uint32_t jobOffsets[maxDrawJobs + maxComputeJobs] = {};

	uint32_t drawJobCounter = 0;
	uint32_t dispatchJobCounter = 0;
	uint32_t combinedJobCounter = 0;

	void SubmitDraw(DrawJob job)
	{
		drawJobsCompact[drawJobCounter] = job;
		jobOffsets[combinedJobCounter] = drawJobCounter;
		drawJobCounter++;

		typesOfJob[combinedJobCounter] = DRAW;
		combinedJobCounter++;
	}

	void SubmitCompute(DispatchJob job)
	{
		dispatchJobsCompact[dispatchJobCounter] = job;
		jobOffsets[combinedJobCounter] = dispatchJobCounter;
		dispatchJobCounter++;

		typesOfJob[combinedJobCounter] = DISPATCH;
		combinedJobCounter++;

		assert(("Depth-stencil buffers are unsupported for compute jobs", !job.hasDepthStencil));
	}
};

JobArray jobs;

void Pipeline::Init(Scene* scenes, uint8_t numScenes)
{
	sceneData = Memory::AllocateArray<SceneMesh>(numScenes);
	numScenesAvailable = 0;

	for (uint32_t i = 0; i < numScenes; i++)
	{
		scenes[i].GetSceneMesh(&sceneData[i].vbuffer, &sceneData[i].ibuffer, &sceneData[i].numIndices);
	}

	// Allocate any textures, buffers, volumes &c we want to use with draws/dispatches here

	// Just one draw for now
	DrawJob job("test.vs", "test.ps");
	job.directToBackbuf = true;
	jobs.SubmitDraw(job);
}

void Pipeline::DeInit()
{
	// Empty for now
}

// Probably going to need more in this than a direct present call ^_^'
void Pipeline::PushFrame(uint32_t sceneID)
{
	D3DWrapper::PrepareBackbuf();
	for (uint32_t i = 0; i < jobs.combinedJobCounter; i++)
	{
		if (jobs.typesOfJob[i] == JobArray::DRAW)
		{
			DrawJob job = jobs.drawJobsCompact[jobs.jobOffsets[i]];
			D3DWrapper::SubmitDraw(job.textures, job.textureBindings, job.numTextures,
								   job.buffers, job.bufferBindings, job.numBuffers,
								   job.volumes, job.volumeBindings, job.numVolumes, job.vs, job.ps, job.directToBackbuf, job.is2D, sceneData[i].vbuffer, sceneData[i].ibuffer, sceneData[i].numIndices);
		}
		else if (jobs.typesOfJob[i] == JobArray::DISPATCH)
		{
			DispatchJob job = jobs.dispatchJobsCompact[jobs.jobOffsets[i]];
			D3DWrapper::SubmitDispatch(job.textures, job.textureBindings, job.numTextures,
									   job.buffers, job.bufferBindings, job.numBuffers,
									   job.volumes, job.volumeBindings, job.numVolumes, job.cs, job.dispatchX, job.dispatchY, job.dispatchZ);
		}
	}

	D3DWrapper::Present();
}
