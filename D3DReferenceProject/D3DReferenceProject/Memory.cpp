#include "Memory.h"
#include <malloc.h>

char* Memory::block = nullptr;

void Memory::Init()
{
	block = (char*)malloc(Memory::initial_alloc);
}

void Memory::DeInit()
{
	free(block);
}
