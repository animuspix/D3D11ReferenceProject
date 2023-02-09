#include "Pipeline.h"
#include "D3DWrapper.h"

// Arrays of resources &c consumed by the pipeline here...
//////////////////////////////////////////////////////////

// Heterogeneous array of jobs consumed by the pipeline
struct DrawJob
{
	// ...
};

struct DispatchJob
{
	// ...
};

struct CopyJob
{
	// ...
};

void Pipeline::Init()
{
	// Empty for now
}

void Pipeline::DeInit()
{
	// Empty for now
}

// Probably going to need more in this than a direct present call ^_^'
void Pipeline::PushFrame()
{
	D3DWrapper::Present();
}
