#include "Memory.h"
#include <malloc.h>
#include <cassert>

char* Memory::block = nullptr;
char* Memory::blockStart = nullptr;

void Memory::Init()
{
	block = (char*)malloc(Memory::initial_alloc);
	blockStart = block;
}

void Memory::DeInit()
{
	free(blockStart);
}

void Memory::FreeToAddress(void* destAddr)
{
	const uint64_t iDestAddr = reinterpret_cast<uint64_t>(destAddr);
	const uint64_t iBlockStart = reinterpret_cast<uint64_t>(blockStart);
	assert((iDestAddr < (iBlockStart + initial_alloc)) && iDestAddr > iBlockStart);

	block = reinterpret_cast<char*>(destAddr); // Memory occupied at destAddr is effectively freed, will be re-used by future allocations
}
